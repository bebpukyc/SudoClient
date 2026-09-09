#include "botsymona.h"

#include <base/io.h>
#include <base/log.h>
#include <base/str.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <game/client/gameclient.h>

static const char *s_apListKeys[CBotSymona::NUM_LISTS] = {"admin", "team", "war", "troll"};

int CBotSymona::RoleOf(const char *pName) const
{
	if(!pName || pName[0] == '\0')
		return ROLE_NONE;

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), pName);
	if(aWanted[0] == '\0')
		return ROLE_NONE;

	char aOwner[MAX_NAME_LENGTH];
	TrimName(aOwner, sizeof(aOwner), g_Config.m_BotOwner);
	if(aOwner[0] != '\0' && str_utf8_comp_nocase(aWanted, aOwner) == 0)
		return ROLE_OWNER;

	if(InList(LIST_ADMIN, aWanted))
		return ROLE_ADMIN;

	return ROLE_NONE;
}

int CBotSymona::RoleOfId(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_aClients[ClientId].m_Active)
		return ROLE_NONE;
	return RoleOf(GameClient()->m_aClients[ClientId].m_aName);
}

bool CBotSymona::InList(int List, const char *pName) const
{
	if(List < 0 || List >= NUM_LISTS || !pName || pName[0] == '\0')
		return false;

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), pName);
	if(aWanted[0] == '\0')
		return false;

	for(const CBotName &Entry : m_avLists[List])
	{
		if(str_utf8_comp_nocase(Entry.m_aName, aWanted) == 0)
			return true;
	}
	return false;
}

bool CBotSymona::ListAdd(int List, const char *pName)
{
	if(List < 0 || List >= NUM_LISTS || !pName)
		return false;

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), pName);
	if(aWanted[0] == '\0')
		return false;
	if(InList(List, aWanted))
		return false;

	CBotName Entry;
	str_copy(Entry.m_aName, aWanted);
	m_avLists[List].push_back(Entry);
	SaveRoles();
	return true;
}

bool CBotSymona::ListRemove(int List, const char *pName)
{
	if(List < 0 || List >= NUM_LISTS || !pName)
		return false;

	char aWanted[MAX_NAME_LENGTH];
	TrimName(aWanted, sizeof(aWanted), pName);
	if(aWanted[0] == '\0')
		return false;

	for(size_t i = 0; i < m_avLists[List].size(); i++)
	{
		if(str_utf8_comp_nocase(m_avLists[List][i].m_aName, aWanted) != 0)
			continue;
		m_avLists[List].erase(m_avLists[List].begin() + i);
		SaveRoles();
		return true;
	}
	return false;
}

int CBotSymona::ListCount(int List) const
{
	if(List < 0 || List >= NUM_LISTS)
		return 0;
	return (int)m_avLists[List].size();
}

const char *CBotSymona::ListEntry(int List, int Index) const
{
	if(List < 0 || List >= NUM_LISTS || Index < 0 || Index >= (int)m_avLists[List].size())
		return "";
	return m_avLists[List][Index].m_aName;
}

void CBotSymona::WriteDefaultRoles()
{
	IOHANDLE File = Storage()->OpenFile(m_aRolesPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	static const char *s_pDefault =
		"# BotSymona - списки игроков. Правится блокнотом или командами в чате.\n"
		"# Формат: ключ = ник        (один ник в строке)\n"
		"#\n"
		"#   admin = ник   может командовать ботом, но не может выдавать team и war\n"
		"#   team  = ник   бот считает его своим и вытаскивает из фриза\n"
		"#   war   = ник   бот с ним воюет\n"
		"#\n"
		"# Владелец задаётся не здесь, а в настройках (bot_owner) - он всегда главный.\n"
		"# Команды в чате, только от владельца:\n"
		"#   !addadmin ник / !deladmin ник / !team ник / !unteam ник / !war ник / !unwar ник\n"
		"\n";

	io_write(File, s_pDefault, str_length(s_pDefault));
	io_close(File);
}

void CBotSymona::LoadRoles()
{
	for(int i = 0; i < NUM_LISTS; i++)
		m_avLists[i].clear();

	IOHANDLE File = Storage()->OpenFile(m_aRolesPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		WriteDefaultRoles();
		return;
	}

	CLineReader Reader;
	if(!Reader.OpenFile(File))
		return;

	while(const char *pLine = Reader.Get())
	{
		char aLine[256];
		str_copy(aLine, pLine);

		char *pKey = aLine;
		while(*pKey == ' ' || *pKey == '\t')
			pKey++;
		if(*pKey == '\0' || *pKey == '#')
			continue;

		char *pEq = (char *)str_find(pKey, "=");
		if(!pEq)
			continue;
		*pEq = '\0';

		char aKey[32];
		TrimName(aKey, sizeof(aKey), pKey);

		for(int i = 0; i < NUM_LISTS; i++)
		{
			if(str_comp_nocase(aKey, s_apListKeys[i]) != 0)
				continue;

			char aName[MAX_NAME_LENGTH];
			TrimName(aName, sizeof(aName), pEq + 1);
			if(aName[0] == '\0' || InList(i, aName))
				break;

			CBotName Entry;
			str_copy(Entry.m_aName, aName);
			m_avLists[i].push_back(Entry);
			break;
		}
	}

	log_info("botsymona", "roles loaded from '%s': %d admin, %d team, %d war", m_aRolesPath,
		ListCount(LIST_ADMIN), ListCount(LIST_TEAM), ListCount(LIST_WAR));
}

void CBotSymona::SaveRoles()
{
	IOHANDLE File = Storage()->OpenFile(m_aRolesPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_info("botsymona", "could not write '%s'", m_aRolesPath);
		return;
	}

	static const char *s_pHeader =
		"# BotSymona - списки игроков. Файл переписывается ботом при каждом изменении,\n"
		"# поэтому свои комментарии сюда добавлять бесполезно - они пропадут.\n"
		"#   admin = ник / team = ник / war = ник\n"
		"\n";
	io_write(File, s_pHeader, str_length(s_pHeader));

	for(int i = 0; i < NUM_LISTS; i++)
	{
		for(const CBotName &Entry : m_avLists[i])
		{
			char aLine[MAX_NAME_LENGTH + 32];
			str_format(aLine, sizeof(aLine), "%s = %s\n", s_apListKeys[i], Entry.m_aName);
			io_write(File, aLine, str_length(aLine));
		}
	}

	io_close(File);
}
