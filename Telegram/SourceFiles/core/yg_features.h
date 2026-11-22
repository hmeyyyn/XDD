#pragma once

#include "base/basic_types.h"
#include "ui/text/text_entity.h"

#include <QtCore/QString>
#include <optional>

class PeerData;

namespace Core::YgFeatures {

[[nodiscard]] bool GhostActiveAny();
[[nodiscard]] bool GhostBlockRead();
[[nodiscard]] bool GhostBlockStories();
[[nodiscard]] bool GhostBlockOnline();
[[nodiscard]] bool GhostBlockTyping();
[[nodiscard]] inline bool GhostDontReadMessages() { return GhostBlockRead(); }
[[nodiscard]] inline bool GhostDontReadStories() { return GhostBlockStories(); }
[[nodiscard]] inline bool GhostDontSendOnline() { return GhostBlockOnline(); }
[[nodiscard]] inline bool GhostDontSendTyping() { return GhostBlockTyping(); }
[[nodiscard]] bool GhostOfflineAfterSend();
[[nodiscard]] bool GhostReadOnSend();

void AllowReadOnSendOnce(uint64 peerId);
[[nodiscard]] bool ConsumeReadOnSendOnce(uint64 peerId);

[[nodiscard]] bool StreamerActive();
[[nodiscard]] bool StreamerCaptureProtected();
[[nodiscard]] bool StreamerHideSensitive();
[[nodiscard]] bool StreamerHidePrivateChats();
[[nodiscard]] bool StreamerHidePrivatePeer(not_null<const PeerData*> peer);
[[nodiscard]] bool StreamerMuteNotifications();
[[nodiscard]] QString MaskSensitiveText(const QString &value);

[[nodiscard]] QString FilterZalgoText(QString text);
void FilterZalgoEntities(TextWithEntities &text);

struct MathReplacement {
	int replaceStart = 0;
	int replaceLength = 0;
	QString replacement;
};
[[nodiscard]] std::optional<MathReplacement> EvaluateInlineMath(
	const QString &text,
	int cursorPosition);

[[nodiscard]] QString EstimateRegistrationDate(uint64 bareUserId);

void InstallSmoothScrollFilter();

} // namespace Core::YgFeatures
