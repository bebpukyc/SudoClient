/* Copyright © 2026 BestProject Team */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_VERSION_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_VERSION_H

#define BESTCLIENT_BUILD_DATE "28.05 00:00"
#define BESTCLIENT_VERSIONNR 100
#define BESTCLIENT_VERSION "2.3 stable-beta"
// Sudo-identity: отдельный тег версии нашей копии. У всех Best-копий базовая
// версия одинаковая, поэтому match по ней метил бы всех подряд — анонсируем
// свой тег, и sudo-детект сравнивает строго с ним.
// Тег семейный, а не версионный — обязан оставаться константой на всех наших
// сборках, иначе sudo перестанет матчиться между ними.
#define SUDOCLIENT_VERSION "release"

#endif
