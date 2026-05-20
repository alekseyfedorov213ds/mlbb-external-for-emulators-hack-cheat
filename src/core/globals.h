#pragma once
#include "contexts.h"

namespace phantom {

extern AppContext      g_app;
extern AuthSession     g_auth;
extern GameContext     g_game;
extern RenderContext   g_render;
extern OverlayConfig   g_overlay;
extern RenderResources g_resources;
extern ChatContext     g_chat;
extern Subscription    g_sub;
extern DaemonContext   g_daemon;

} // namespace phantom
