#pragma once
#include <cstdint>

namespace fluxerpp::permissions {

// Discord permission flags mapped to Stoat/Revolt equivalents for compatibility
constexpr int64_t CreateInstantInvite = 1ULL << 0;
constexpr int64_t KickMembers          = 1ULL << 1;
constexpr int64_t BanMembers           = 1ULL << 2;
constexpr int64_t Administrator        = 1ULL << 3;
constexpr int64_t ManageChannels       = 1ULL << 4;
constexpr int64_t ManageServer         = 1ULL << 5; // Manage Guild
constexpr int64_t AddReactions         = 1ULL << 6;
constexpr int64_t ViewAuditLog         = 1ULL << 7;
constexpr int64_t PrioritySpeaker      = 1ULL << 8;
constexpr int64_t Stream               = 1ULL << 9;
constexpr int64_t ViewChannel          = 1ULL << 10;
constexpr int64_t SendMessages         = 1ULL << 11;
constexpr int64_t SendTTSMessages      = 1ULL << 12;
constexpr int64_t ManageMessages       = 1ULL << 13;
constexpr int64_t EmbedLinks           = 1ULL << 14;
constexpr int64_t UploadFiles          = 1ULL << 15;
constexpr int64_t ReadMessageHistory   = 1ULL << 16;
constexpr int64_t MentionEveryone      = 1ULL << 17;
constexpr int64_t UseExternalEmojis    = 1ULL << 18;
constexpr int64_t ViewGuildInsights    = 1ULL << 19;
constexpr int64_t Connect              = 1ULL << 20;
constexpr int64_t Speak                = 1ULL << 21;
constexpr int64_t MuteMembers          = 1ULL << 22;
constexpr int64_t DeafenMembers        = 1ULL << 23;
constexpr int64_t MoveMembers          = 1ULL << 24;
constexpr int64_t UseVAD               = 1ULL << 25;
constexpr int64_t ChangeNickname       = 1ULL << 26;
constexpr int64_t ManageNicknames      = 1ULL << 27;
constexpr int64_t ManagePermissions    = 1ULL << 28; // Manage Roles
constexpr int64_t ManageRole           = 1ULL << 28; // Manage Roles
constexpr int64_t ManageWebhooks       = 1ULL << 29;
constexpr int64_t ManageCustomisation  = 1ULL << 30; // Custom placeholder
constexpr int64_t ManageEmojisAndStickers = 1ULL << 30;
constexpr int64_t UseApplicationCommands = 1ULL << 31;
constexpr int64_t RequestToSpeak       = 1ULL << 32;
constexpr int64_t ManageEvents         = 1ULL << 33;
constexpr int64_t ManageThreads        = 1ULL << 34;
constexpr int64_t CreatePublicThreads  = 1ULL << 35;
constexpr int64_t CreatePrivateThreads = 1ULL << 36;
constexpr int64_t UseExternalStickers  = 1ULL << 37;
constexpr int64_t SendMessagesInThreads = 1ULL << 38;
constexpr int64_t UseEmbeddedActivities = 1ULL << 39;
constexpr int64_t TimeoutMembers       = 1ULL << 40; // Moderate Members

// Compatibility alias for Revolt's Masquerade permission
constexpr int64_t Masquerade           = 1ULL << 29; // Map to ManageWebhooks since webhooks are used for masquerades on Discord/Fluxer
constexpr int64_t CreateInvites        = CreateInstantInvite;

} // namespace fluxerpp::permissions
