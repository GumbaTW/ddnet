/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "friends.h"

#include <base/dbg.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/config.h>

CFriends::CFriends()
{
	mem_zero(m_aFriends, sizeof(m_aFriends));
	mem_zero(m_aFolders, sizeof(m_aFolders));
	m_NumFriends = 0;
	m_NumFolders = 0;
	m_Foes = false;
}

void CFriends::ConAddFriend(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->AddFriend(pResult->GetString(0), pResult->GetString(1), pResult->GetString(2));
}

void CFriends::ConRemoveFriend(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->RemoveFriend(pResult->GetString(0), pResult->GetString(1));
}

void CFriends::ConFriends(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->Friends();
}

void CFriends::ConAddFriendFolder(IConsole::IResult *pResult, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	pSelf->AddFolder(pResult->GetString(0));
}

void CFriends::Init(bool Foes)
{
	m_Foes = Foes;

	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this);

	IConsole *pConsole = Kernel()->RequestInterface<IConsole>();
	if(pConsole)
	{
		if(Foes)
		{
			pConsole->Register("add_foe", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConAddFriend, this, "Add a foe");
			pConsole->Register("remove_foe", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveFriend, this, "Remove a foe");
			pConsole->Register("foes", "", CFGFLAG_CLIENT, ConFriends, this, "List foes");
		}
		else
		{
			// The folder argument is optional so existing "add_friend \"name\" \"clan\"" lines still load.
			// A stock client ignores a third argument and drops it the next time it saves settings.
			pConsole->Register("add_friend", "s[name] ?s[clan] ?s[folder]", CFGFLAG_CLIENT, ConAddFriend, this, "Add a friend");
			pConsole->Register("remove_friend", "s[name] ?s[clan]", CFGFLAG_CLIENT, ConRemoveFriend, this, "Remove a friend");
			pConsole->Register("friends", "", CFGFLAG_CLIENT, ConFriends, this, "List friends");
			pConsole->Register("add_friend_folder", "s[folder]", CFGFLAG_CLIENT, ConAddFriendFolder, this, "Add a friend list folder");
		}
	}
}

const CFriendInfo *CFriends::GetFriend(int Index) const
{
	dbg_assert(Index >= 0 && Index < m_NumFriends, "Invalid Index: %d", Index);
	return &m_aFriends[Index];
}

int CFriends::FindFriend(const char *pName, const char *pClan) const
{
	const unsigned NameHash = str_quickhash(pName);
	const unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName)) &&
			((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan))))
			return i;
	}
	return -1;
}

int CFriends::FindFolder(const char *pFolder) const
{
	for(int i = 0; i < m_NumFolders; ++i)
	{
		if(str_comp_nocase(m_aFolders[i], pFolder) == 0)
			return i;
	}
	return -1;
}

int CFriends::GetFriendState(const char *pName, const char *pClan) const
{
	int Result = FRIEND_NO;
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan)))
		{
			if(m_aFriends[i].m_aName[0] == 0)
			{
				Result = FRIEND_CLAN;
			}
			else if(m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName))
			{
				Result = FRIEND_PLAYER;
				break;
			}
		}
	}
	return Result;
}

bool CFriends::IsFriend(const char *pName, const char *pClan, bool PlayersOnly) const
{
	unsigned NameHash = str_quickhash(pName);
	unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if(((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan))) &&
			((!PlayersOnly && m_aFriends[i].m_aName[0] == 0) || (m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName))))
			return true;
	}
	return false;
}

void CFriends::AddFriend(const char *pName, const char *pClan, const char *pFolder)
{
	if(pName[0] == 0 && pClan[0] == 0)
		return;
	if(pFolder == nullptr || m_Foes)
		pFolder = "";

	if(FindFriend(pName, pClan) >= 0)
	{
		if(pFolder[0] != '\0')
			SetFriendFolder(pName, pClan, pFolder);
		return;
	}
	if(m_NumFriends == MAX_FRIENDS)
		return;

	str_copy(m_aFriends[m_NumFriends].m_aName, pName);
	str_copy(m_aFriends[m_NumFriends].m_aClan, pClan);
	m_aFriends[m_NumFriends].m_aFolder[0] = '\0';
	m_aFriends[m_NumFriends].m_NameHash = str_quickhash(pName);
	m_aFriends[m_NumFriends].m_ClanHash = str_quickhash(pClan);
	++m_NumFriends;
	if(pFolder[0] != '\0')
		SetFriendFolder(pName, pClan, pFolder);
}

void CFriends::RemoveFriend(const char *pName, const char *pClan)
{
	const int Index = FindFriend(pName, pClan);
	if(Index >= 0)
		RemoveFriend(Index);
}

static bool NormalizeFriendFolderName(char (&aOut)[CFriendInfo::MAX_FOLDER_LENGTH], const char *pIn)
{
	if(pIn == nullptr)
		return false;
	while(*pIn == ' ')
		++pIn;
	if(pIn[0] == '\0' || str_length(pIn) >= CFriendInfo::MAX_FOLDER_LENGTH)
		return false;

	str_copy(aOut, pIn);
	int Length = str_length(aOut);
	while(Length > 0 && aOut[Length - 1] == ' ')
	{
		--Length;
		aOut[Length] = '\0';
	}
	if(aOut[0] == '\0')
		return false;
	for(const char *pChar = aOut; *pChar; ++pChar)
	{
		if(*pChar == '\n' || *pChar == '\r')
			return false;
	}
	return true;
}

bool CFriends::SetFriendFolder(const char *pName, const char *pClan, const char *pFolder)
{
	if(m_Foes)
		return false;
	const int FriendIndex = FindFriend(pName, pClan);
	if(FriendIndex < 0)
		return false;

	char aFolder[CFriendInfo::MAX_FOLDER_LENGTH];
	if(!NormalizeFriendFolderName(aFolder, pFolder))
	{
		if(pFolder == nullptr || pFolder[0] == '\0')
		{
			m_aFriends[FriendIndex].m_aFolder[0] = '\0';
			return true;
		}
		// Spaces-only clears the folder. Anything else that failed to normalize is rejected.
		const char *pCursor = pFolder;
		while(*pCursor == ' ')
			++pCursor;
		if(pCursor[0] == '\0')
		{
			m_aFriends[FriendIndex].m_aFolder[0] = '\0';
			return true;
		}
		return false;
	}

	if(FindFolder(aFolder) < 0 && !AddFolder(aFolder))
		return false;
	const int FolderIndex = FindFolder(aFolder);
	if(FolderIndex < 0)
		return false;
	str_copy(m_aFriends[FriendIndex].m_aFolder, m_aFolders[FolderIndex]);
	return true;
}

const char *CFriends::FriendFolder(const char *pName, const char *pClan) const
{
	const char *pResult = "";
	const unsigned NameHash = str_quickhash(pName);
	const unsigned ClanHash = str_quickhash(pClan);
	for(int i = 0; i < m_NumFriends; ++i)
	{
		if((g_Config.m_ClFriendsIgnoreClan && m_aFriends[i].m_aName[0]) || (m_aFriends[i].m_ClanHash == ClanHash && !str_comp(m_aFriends[i].m_aClan, pClan)))
		{
			if(m_aFriends[i].m_aName[0] == 0)
			{
				pResult = m_aFriends[i].m_aFolder;
			}
			else if(m_aFriends[i].m_NameHash == NameHash && !str_comp(m_aFriends[i].m_aName, pName))
			{
				return m_aFriends[i].m_aFolder;
			}
		}
	}
	return pResult;
}

const char *CFriends::GetFolder(int Index) const
{
	dbg_assert(Index >= 0 && Index < m_NumFolders, "Invalid folder index: %d", Index);
	return m_aFolders[Index];
}

bool CFriends::AddFolder(const char *pFolder)
{
	if(m_Foes)
		return false;

	char aFolder[CFriendInfo::MAX_FOLDER_LENGTH];
	if(!NormalizeFriendFolderName(aFolder, pFolder))
		return false;
	if(FindFolder(aFolder) >= 0)
		return false;
	if(m_NumFolders >= MAX_FRIEND_FOLDERS)
		return false;

	str_copy(m_aFolders[m_NumFolders], aFolder);
	++m_NumFolders;
	return true;
}

bool CFriends::RemoveFolder(const char *pFolder)
{
	if(m_Foes)
		return false;
	const int Index = FindFolder(pFolder);
	if(Index < 0)
		return false;

	for(int i = 0; i < m_NumFriends; ++i)
	{
		if(str_comp_nocase(m_aFriends[i].m_aFolder, m_aFolders[Index]) == 0)
			m_aFriends[i].m_aFolder[0] = '\0';
	}
	mem_move(&m_aFolders[Index], &m_aFolders[Index + 1], sizeof(m_aFolders[0]) * (m_NumFolders - (Index + 1)));
	--m_NumFolders;
	return true;
}

void CFriends::RemoveFriend(int Index)
{
	dbg_assert(Index >= 0 && Index < m_NumFriends, "Invalid Index: %d", Index);
	mem_move(&m_aFriends[Index], &m_aFriends[Index + 1], sizeof(CFriendInfo) * (m_NumFriends - (Index + 1)));
	--m_NumFriends;
}

void CFriends::Friends()
{
	char aBuf[128];
	IConsole *pConsole = Kernel()->RequestInterface<IConsole>();
	if(pConsole)
	{
		for(int i = 0; i < m_NumFriends; ++i)
		{
			if(m_aFriends[i].m_aFolder[0] != '\0')
				str_format(aBuf, sizeof(aBuf), "Name: %s, Clan: %s, Folder: %s", m_aFriends[i].m_aName, m_aFriends[i].m_aClan, m_aFriends[i].m_aFolder);
			else
				str_format(aBuf, sizeof(aBuf), "Name: %s, Clan: %s", m_aFriends[i].m_aName, m_aFriends[i].m_aClan);

			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, m_Foes ? "foes" : "friends", aBuf, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor)));
		}
	}
}

void CFriends::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CFriends *pSelf = (CFriends *)pUserData;
	char aBuf[512];
	const char *pEnd = aBuf + sizeof(aBuf) - 4;

	// Unknown to the stock client, so that build keeps these lines when it rewrites settings.cfg.
	if(!pSelf->m_Foes)
	{
		for(int i = 0; i < pSelf->m_NumFolders; ++i)
		{
			str_copy(aBuf, "add_friend_folder \"");
			char *pDst = aBuf + str_length(aBuf);
			str_escape(&pDst, pSelf->m_aFolders[i], pEnd);
			str_append(aBuf, "\"");
			pConfigManager->WriteLine(aBuf);
		}
	}

	for(int i = 0; i < pSelf->m_NumFriends; ++i)
	{
		str_copy(aBuf, pSelf->m_Foes ? "add_foe " : "add_friend ");

		str_append(aBuf, "\"");
		char *pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pSelf->m_aFriends[i].m_aName, pEnd);
		str_append(aBuf, "\" \"");
		pDst = aBuf + str_length(aBuf);
		str_escape(&pDst, pSelf->m_aFriends[i].m_aClan, pEnd);
		str_append(aBuf, "\"");

		// Only written when set, so friends without a folder stay on the old two-argument line.
		if(!pSelf->m_Foes && pSelf->m_aFriends[i].m_aFolder[0] != '\0')
		{
			str_append(aBuf, " \"");
			pDst = aBuf + str_length(aBuf);
			str_escape(&pDst, pSelf->m_aFriends[i].m_aFolder, pEnd);
			str_append(aBuf, "\"");
		}

		pConfigManager->WriteLine(aBuf);
	}
}
