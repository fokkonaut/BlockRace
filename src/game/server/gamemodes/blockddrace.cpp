/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/flag.h>
#include "blockddrace.h"
#include "gamemode.h"

CGameControllerBlockDDrace::CGameControllerBlockDDrace(class CGameContext *pGameServer) :
		IGameController(pGameServer), m_Teams(pGameServer)
{
	// BlockDDrace

	m_apFlags[0] = 0;
	m_apFlags[1] = 0;

	m_GameFlags = GAMEFLAG_FLAGS;

	m_pGameType = g_Config.m_SvTestingCommands ? TEST_NAME : GAME_NAME;

	InitTeleporter();
}

CGameControllerBlockDDrace::~CGameControllerBlockDDrace()
{
	// Nothing to clean
}

void CGameControllerBlockDDrace::Tick()
{
	IGameController::Tick();
}

void CGameControllerBlockDDrace::InitTeleporter()
{
	if (!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for (int i = 0; i < Width * Height; i++)
	{
		int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
		int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
		if (Number > 0)
		{
			if (Type == TILE_TELEOUT)
			{
				m_TeleOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if (Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
		}
	}
}


/*************************************************
*                                                *
*              B L O C K D D R A C E             *
*                                                *
**************************************************/

bool CGameControllerBlockDDrace::OnEntity(int Index, vec2 Pos)
{
	int Team = -1;
	if (Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if (Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if (Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

int CGameControllerBlockDDrace::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int HadFlag = 0;

	// drop flags
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if (!F || !pKiller || !pKiller->GetCharacter())
			continue;

		if (F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if (F->GetCarrier() == pVictim)
		{
			if (HasFlag(pKiller->GetCharacter()) != -1)
				F->Drop();
			HadFlag |= 1;
		}
	}

	return HadFlag;
}

void CGameControllerBlockDDrace::ChangeFlagOwner(CCharacter *pOldCarrier, CCharacter *pNewCarrier)
{
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];

		if (!F || !pNewCarrier)
			return;

		if (F->GetCarrier() == pOldCarrier && HasFlag(pNewCarrier) == -1)
			F->Grab(pNewCarrier);
	}
}

void CGameControllerBlockDDrace::ForceFlagOwner(int ClientID, int Team)
{
	CFlag *F = m_apFlags[Team];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!F || (Team != TEAM_RED && Team != TEAM_BLUE) || (!pChr && ClientID >= 0))
		return;
	if (ClientID >= 0 && HasFlag(pChr) == -1)
	{
		if (F->GetCarrier())
			F->SetLastCarrier(F->GetCarrier());
		F->Grab(pChr);
	}
	else if (ClientID == -1)
		F->Reset();
}

int CGameControllerBlockDDrace::HasFlag(CCharacter *pChr)
{
	if (!pChr)
		return -1;

	for (int i = 0; i < 2; i++)
	{
		if (!m_apFlags[i])
			continue;

		if (m_apFlags[i]->GetCarrier() == pChr)
			return i;
	}
	return -1;
}

void CGameControllerBlockDDrace::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if (!pGameDataObj)
		return;

	// BlockDDrace
	CPlayer* pSnap = GameServer()->m_apPlayers[SnappingClient];

	bool FlagPosFix[2];
	FlagPosFix[TEAM_RED] = false;
	FlagPosFix[TEAM_BLUE] = false;
	if (pSnap->m_SnapFixDDNet || pSnap->m_ClientVersion < VERSION_DDNET_OLD)
		for (int i = 0; i < 2; i++)
			if (
				m_apFlags[i]
				&& m_apFlags[i]->GetCarrier() && m_apFlags[i]->GetCarrier()->GetPlayer()
				&& m_apFlags[i]->GetCarrier()->GetPlayer() == pSnap
				)
				FlagPosFix[i] = true;

	if (m_apFlags[TEAM_RED])
	{
		if (m_apFlags[TEAM_RED]->IsAtStand())
			pGameDataObj->m_FlagCarrierRed = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
		{
			if (!pSnap->m_SnapFixDDNet && pSnap->m_ClientVersion >= VERSION_DDNET_OLD)
				pGameDataObj->m_FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
			else if (FlagPosFix[TEAM_RED])
				pGameDataObj->m_FlagCarrierRed = 0;
			else
				pGameDataObj->m_FlagCarrierRed = FLAG_TAKEN;
		}
		else
			pGameDataObj->m_FlagCarrierRed = FLAG_TAKEN;
	}
	else
		pGameDataObj->m_FlagCarrierRed = FLAG_MISSING;
	if (m_apFlags[TEAM_BLUE])
	{
		if (m_apFlags[TEAM_BLUE]->IsAtStand())
			pGameDataObj->m_FlagCarrierBlue = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
		{
			if (!pSnap->m_SnapFixDDNet && pSnap->m_ClientVersion >= VERSION_DDNET_OLD)
				pGameDataObj->m_FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
			else if (FlagPosFix[TEAM_BLUE])
				pGameDataObj->m_FlagCarrierBlue = 0;
			else
				pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;
		}
		else
			pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;
	}
	else
		pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
}