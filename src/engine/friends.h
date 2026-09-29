/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_FRIENDS_H
#define ENGINE_FRIENDS_H

#include "kernel.h"

#include <engine/shared/protocol.h>

struct CFriendInfo
{
	// Includes the terminating null. Kept short so an add_friend line still fits a config row.
	static constexpr int MAX_FOLDER_LENGTH = 32;

	char m_aName[MAX_NAME_LENGTH];
	char m_aClan[MAX_CLAN_LENGTH];
	char m_aFolder[MAX_FOLDER_LENGTH];
	unsigned m_NameHash;
	unsigned m_ClanHash;
};

class IFriends : public IInterface
{
	MACRO_INTERFACE("friends")
public:
	enum
	{
		FRIEND_NO = 0,
		FRIEND_CLAN,
		FRIEND_PLAYER,
	};
	static constexpr auto MAX_FRIENDS = 4096;
	static constexpr auto MAX_FRIEND_FOLDERS = 64;

	virtual void Init(bool Foes = false) = 0;

	virtual int NumFriends() const = 0;
	virtual const CFriendInfo *GetFriend(int Index) const = 0;
	virtual int GetFriendState(const char *pName, const char *pClan) const = 0;
	virtual bool IsFriend(const char *pName, const char *pClan, bool PlayersOnly) const = 0;

	// Empty pFolder means the friend is not in a folder. Old two-argument lines still load.
	virtual void AddFriend(const char *pName, const char *pClan, const char *pFolder = "") = 0;
	virtual void RemoveFriend(const char *pName, const char *pClan) = 0;
	virtual bool SetFriendFolder(const char *pName, const char *pClan, const char *pFolder) = 0;
	// Folder of the matching player entry, or of the clan entry when only the clan is a friend.
	virtual const char *FriendFolder(const char *pName, const char *pClan) const = 0;

	virtual int NumFolders() const = 0;
	virtual const char *GetFolder(int Index) const = 0;
	virtual bool AddFolder(const char *pFolder) = 0;
	// Drops the folder and leaves its friends on the list, ungrouped.
	virtual bool RemoveFolder(const char *pFolder) = 0;
};

#endif
