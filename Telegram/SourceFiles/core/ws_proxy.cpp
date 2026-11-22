#include "core/ws_proxy.h"

#include "base/random.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/enhanced_settings.h"
#include "core/websocket_client.h"
#include "lang/lang_keys.h"
#include "mtproto/mtproto_proxy_data.h"
#include "rpl/event_stream.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QPointer>
#include <QtCore/QtEndian>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

extern "C" {
#include <openssl/evp.h>
} // extern "C"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace Core::WsProxy {
namespace {

constexpr auto kBasePort = 1443;
constexpr auto kPortTries = 24;
constexpr auto kSecretPref = "yanderegram.ws_proxy.secret";

class AesCtr final {
public:
	AesCtr() : _ctx(EVP_CIPHER_CTX_new()) {}
	~AesCtr() {
		if (_ctx) EVP_CIPHER_CTX_free(_ctx);
	}
	AesCtr(const AesCtr &) = delete;
	AesCtr &operator=(const AesCtr &) = delete;

	bool init(const uchar *key32, const uchar *iv16) {
		return _ctx && (EVP_EncryptInit_ex(_ctx, EVP_aes_256_ctr(), nullptr, key32, iv16) == 1);
	}

	QByteArray process(const QByteArray &in) {
		if (!_ctx || in.isEmpty()) return {};
		QByteArray out(in.size(), Qt::Uninitialized);
		int len = 0;
		EVP_EncryptUpdate(
			_ctx,
			reinterpret_cast<uchar*>(out.data()),
			&len,
			reinterpret_cast<const uchar*>(in.constData()),
			in.size());
		return out;
	}

private:
	EVP_CIPHER_CTX *_ctx = nullptr;
};

[[nodiscard]] bool IsGoodRelayNonce(const std::array<uchar, 64> &r) {
	if (r[0] == 0xEF) return false;
	uint32 head = 0;
	std::memcpy(&head, r.data(), 4);
	if (head == 0x44414548u || head == 0x54534F50u || head == 0x20544547u
		|| head == 0xEEEEEEEEu || head == 0xDDDDDDDDu
		|| head == 0x02010316u || head == 0x4954504Fu) {
		return false;
	}
	uint32 second = 0;
	std::memcpy(&second, r.data() + 4, 4);
	return second != 0u;
}

class BridgeSession final : public QObject {
public:
	BridgeSession(
		QTcpSocket *client,
		const QByteArray &secret,
		Fn<void()> onStateChanged,
		QObject *parent)
	: QObject(parent)
	, _client(client)
	, _secret(secret)
	, _notify(std::move(onStateChanged)) {
		_client->setParent(this);
		connect(_client, &QTcpSocket::readyRead, this, [=] { onClientData(); });
		connect(_client, &QTcpSocket::disconnected, this, [=] { shutdown(); });
		connect(_client, &QAbstractSocket::errorOccurred, this, [=](QAbstractSocket::SocketError) {
			shutdown();
		});
	}

	~BridgeSession() override {
		if (_ws) _ws->close();
	}

	[[nodiscard]] bool connected() const { return _wsConnected; }

private:
	void onClientData() {
		if (_closed || !_client) return;
		_tcpBuf.append(_client->readAll());
		if (!_handshakeDone) {
			if (_tcpBuf.size() < 64) return;
			const auto hs = _tcpBuf.left(64);
			_tcpBuf.remove(0, 64);
			if (!parseHandshake(hs)) {
				shutdown();
				return;
			}
			_handshakeDone = true;
		}
		if (!_tcpBuf.isEmpty()) {
			const auto cipher = _tcpBuf;
			_tcpBuf.clear();
			const auto plain = _cltDec.process(cipher);
			const auto reenc = _tgEnc.process(plain);
			enqueueOrSendFrames(cipher, plain, reenc);
		}
	}

	bool parseHandshake(const QByteArray &hs) {
		const auto decPre = hs.mid(8, 32);
		const auto decIv = hs.mid(40, 16);
		auto rev = hs.mid(8, 48);
		std::reverse(rev.begin(), rev.end());
		const auto encPre = rev.left(32);
		const auto encIv = rev.mid(32, 16);

		const auto decKey = QCryptographicHash::hash(decPre + _secret, QCryptographicHash::Sha256);
		const auto encKey = QCryptographicHash::hash(encPre + _secret, QCryptographicHash::Sha256);

		if (!_cltDec.init(reinterpret_cast<const uchar*>(decKey.constData()),
				reinterpret_cast<const uchar*>(decIv.constData()))
			|| !_cltEnc.init(reinterpret_cast<const uchar*>(encKey.constData()),
				reinterpret_cast<const uchar*>(encIv.constData()))) {
			return false;
		}
		const auto plain = _cltDec.process(hs);
		const auto tag = qFromLittleEndian<quint32>(plain.constData() + 56);
		if (tag != 0xEFEFEFEFu && tag != 0xEEEEEEEEu && tag != 0xDDDDDDDDu) {
			return false;
		}
		_protoTag = tag;
		const auto dcIdx = qFromLittleEndian<qint16>(plain.constData() + 60);
		auto dc = int(std::abs(int(dcIdx)));
		if (dc == 203) dc = 2;
		if (dc < 1 || dc > 5) dc = 2;
		const auto isMedia = (dcIdx < 0);

		// Build un-secreted outbound relay_init for Telegram WebSocket.
		std::array<uchar, 64> rnd{};
		do {
			base::RandomFill(rnd.data(), rnd.size());
		} while (!IsGoodRelayNonce(rnd));

		std::memcpy(rnd.data() + 56, plain.constData() + 56, 4);
		qToLittleEndian<qint16>(dcIdx, rnd.data() + 60);

		std::array<uchar, 48> tgRev{};
		std::memcpy(tgRev.data(), rnd.data() + 8, 48);
		std::reverse(tgRev.begin(), tgRev.end());

		if (!_tgEnc.init(rnd.data() + 8, rnd.data() + 40)
			|| !_tgDec.init(tgRev.data(), tgRev.data() + 32)) {
			return false;
		}
		const auto rawInit = QByteArray(reinterpret_cast<const char*>(rnd.data()), 64);
		const auto encInit = _tgEnc.process(rawInit);
		_relayInit = rawInit.left(56) + encInit.mid(56, 8);

		const auto host = isMedia
			? u"kws%1-1.web.telegram.org"_q.arg(dc)
			: u"kws%1.web.telegram.org"_q.arg(dc);

		_ws = std::make_unique<WebSocketClient>(this);
		_ws->onConnected = [=] {
			if (_closed) return;
			_wsConnected = true;
			_ws->sendBinary(_relayInit);
			for (const auto &frame : _pendingFrames) {
				_ws->sendBinary(frame);
			}
			_pendingFrames.clear();
			if (_notify) _notify();
		};
		_ws->onBinary = [=](const QByteArray &data) {
			if (_closed || !_client || data.isEmpty()) return;
			const auto plainIn = _tgDec.process(data);
			const auto toClient = _cltEnc.process(plainIn);
			_client->write(toClient);
		};
		_ws->onClosed = [=] { shutdown(); };
		_ws->connectTo(host, 443, u"/apiws"_q, u"binary"_q);
		return true;
	}

	void enqueueOrSendFrames(
			const QByteArray &,
			const QByteArray &plain,
			const QByteArray &reenc) {
		if (_splitDisabled) {
			sendFrame(reenc);
			return;
		}
		_plainStream.append(plain);
		_cipherStream.append(reenc);
		int offset = 0;
		const int total = _plainStream.size();
		while (offset < total) {
			const auto pktLen = nextPacketLength(offset, total - offset);
			if (pktLen < 0) break; // need more bytes
			if (pktLen == 0) {
				sendFrame(_cipherStream.mid(offset));
				offset = total;
				_splitDisabled = true;
				break;
			}
			if (offset + pktLen > total) break;
			sendFrame(_cipherStream.mid(offset, pktLen));
			offset += pktLen;
		}
		if (offset > 0) {
			_plainStream.remove(0, offset);
			_cipherStream.remove(0, offset);
		}
	}

	int nextPacketLength(int offset, int avail) const {
		const auto *p = reinterpret_cast<const uchar*>(_plainStream.constData() + offset);
		if (_protoTag == 0xEFEFEFEFu) {
			const auto first = p[0];
			if (first == 0x7Fu || first == 0xFFu) {
				if (avail < 4) return -1;
				const int words = int(p[1]) | (int(p[2]) << 8) | (int(p[3]) << 16);
				return (words <= 0 || words > 4 * 1024 * 1024) ? 0 : (4 + words * 4);
			}
			const int words = int(first & 0x7Fu);
			return (words <= 0) ? 0 : (1 + words * 4);
		}
		if (_protoTag == 0xEEEEEEEEu || _protoTag == 0xDDDDDDDDu) {
			if (avail < 4) return -1;
			const auto len = qFromLittleEndian<quint32>(p) & 0x7FFFFFFFu;
			return (len == 0 || len > 16 * 1024 * 1024) ? 0 : int(4 + len);
		}
		return 0;
	}

	void sendFrame(const QByteArray &frame) {
		if (frame.isEmpty()) return;
		if (_wsConnected && _ws) {
			_ws->sendBinary(frame);
		} else {
			_pendingFrames.push_back(frame);
		}
	}

	void shutdown() {
		if (_closed) return;
		_closed = true;
		const auto wasConnected = _wsConnected;
		_wsConnected = false;
		if (_ws) _ws->close();
		if (_client) _client->disconnectFromHost();
		if (wasConnected && _notify) _notify();
		deleteLater();
	}

	QPointer<QTcpSocket> _client;
	QByteArray _secret;
	Fn<void()> _notify;
	std::unique_ptr<WebSocketClient> _ws;
	AesCtr _cltDec;
	AesCtr _cltEnc;
	AesCtr _tgEnc;
	AesCtr _tgDec;
	QByteArray _tcpBuf;
	QByteArray _relayInit;
	QByteArray _plainStream;
	QByteArray _cipherStream;
	std::vector<QByteArray> _pendingFrames;
	quint32 _protoTag = 0;
	bool _handshakeDone = false;
	bool _wsConnected = false;
	bool _splitDisabled = false;
	bool _closed = false;
};

struct ManagerState {
	std::unique_ptr<QTcpServer> server;
	QByteArray secretBytes;
	QString secretHex;
	int port = 0;
	std::vector<QPointer<BridgeSession>> sessions;
	rpl::event_stream<> changes;
	rpl::lifetime lifetime;
	bool started = false;
};

ManagerState &State() {
	static auto state = ManagerState();
	return state;
}

void EnsureSecret() {
	auto &s = State();
	if (s.secretBytes.size() == 16) return;
	const auto saved = QString::fromLatin1(
		Core::App().settings().readPref<QByteArray>(kSecretPref)).trimmed();
	const auto raw = QByteArray::fromHex(saved.toLatin1());
	if (raw.size() == 16) {
		s.secretBytes = raw;
		s.secretHex = saved.toLower();
		return;
	}
	std::array<uchar, 16> gen{};
	base::RandomFill(gen.data(), gen.size());
	s.secretBytes = QByteArray(reinterpret_cast<const char*>(gen.data()), 16);
	s.secretHex = QString::fromLatin1(s.secretBytes.toHex());
	Core::App().settings().writePref<QByteArray>(kSecretPref, s.secretHex.toLatin1());
	Core::App().saveSettingsDelayed();
}

MTP::ProxyData LocalProxyData() {
	auto &s = State();
	EnsureSecret();
	auto p = MTP::ProxyData();
	p.type = MTP::ProxyData::Type::Mtproto;
	p.host = u"127.0.0.1"_q;
	p.port = uint32(s.port);
	p.password = s.secretHex;
	return p;
}

bool IsOurProxy(const MTP::ProxyData &p) {
	EnsureSecret();
	return p.type == MTP::ProxyData::Type::Mtproto
		&& p.host == u"127.0.0.1"_q
		&& p.password.toLower() == State().secretHex;
}

void PruneSessions() {
	auto &list = State().sessions;
	list.erase(
		std::remove_if(list.begin(), list.end(), [](const QPointer<BridgeSession> &p) {
			return p.isNull();
		}),
		list.end());
}

void StartServer() {
	auto &s = State();
	EnsureSecret();
	if (!s.server) {
		s.server = std::make_unique<QTcpServer>();
		QObject::connect(s.server.get(), &QTcpServer::newConnection, [] {
			auto &st = State();
			while (st.server && st.server->hasPendingConnections()) {
				auto *sock = st.server->nextPendingConnection();
				if (!sock) continue;
				auto *bridge = new BridgeSession(sock, st.secretBytes, [] {
					PruneSessions();
					State().changes.fire({});
				}, st.server.get());
				st.sessions.push_back(bridge);
			}
			PruneSessions();
			st.changes.fire({});
		});
	}
	if (!s.server->isListening()) {
		s.port = 0;
		for (int i = 0; i < kPortTries; ++i) {
			const auto candidate = quint16(kBasePort + i);
			if (s.server->listen(QHostAddress::LocalHost, candidate)) {
				s.port = int(candidate);
				break;
			}
		}
		if (!s.port && s.server->listen(QHostAddress::LocalHost, 0)) {
			s.port = int(s.server->serverPort());
		}
	}
	if (s.port > 0) {
		const auto proxy = LocalProxyData();
		auto &proxySettings = Core::App().settings().proxy();
		auto &list = proxySettings.list();
		list.erase(
			std::remove_if(list.begin(), list.end(), IsOurProxy),
			list.end());
		list.push_back(proxy);
		Core::App().setCurrentProxy(proxy, MTP::ProxyData::Settings::Enabled);
		Core::App().saveSettingsDelayed();
	}
	s.changes.fire({});
}

void StopServer() {
	auto &s = State();
	if (s.server) {
		s.server->close();
		s.server.reset();
	}
	s.sessions.clear();
	s.port = 0;
	auto &proxySettings = Core::App().settings().proxy();
	if (IsOurProxy(proxySettings.selected())) {
		Core::App().setCurrentProxy(
			proxySettings.selected(),
			MTP::ProxyData::Settings::Disabled);
		Core::App().saveSettingsDelayed();
	}
	s.changes.fire({});
}

} // namespace

void Start() {
	auto &s = State();
	if (s.started) return;
	s.started = true;
	if (Enabled()) {
		StartServer();
	}
	EnhancedSettings::Changes(
		EnhancedSettings::Option::WsProxyEnabled
	) | rpl::on_next([] {
		if (Enabled()) {
			StartServer();
		} else {
			StopServer();
		}
	}, s.lifetime);
}

void SetEnabled(bool enabled) {
	if (Enabled() == enabled) return;
	EnhancedSettings::Set(EnhancedSettings::Option::WsProxyEnabled, enabled);
}

bool Enabled() {
	return EnhancedSettings::Get(EnhancedSettings::Option::WsProxyEnabled);
}

int Port() {
	return State().port;
}

int ActiveSessions() {
	PruneSessions();
	int count = 0;
	for (const auto &p : State().sessions) {
		if (p && p->connected()) ++count;
	}
	return count;
}

QString StatusText() {
	if (!Enabled() || !State().port) {
		return tr::lng_yg_ws_proxy_off(tr::now);
	}
	return tr::lng_yg_ws_proxy_on(
		tr::now,
		lt_port,
		QString::number(State().port),
		lt_count,
		QString::number(ActiveSessions()));
}

rpl::producer<> Changes() {
	return State().changes.events();
}

} // namespace Core::WsProxy
