/*************************************************
*                                                *
*              B L O C K D D R A C E             *
*                                                *
**************************************************/

#include <game/server/gamecontext.h>
#include "stable_projectile.h"
#include <game/server/teams.h>

CStableProjectile::CStableProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, bool HideOnSpec)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_STABLE_PROJECTILE)
{
	m_Type = GameServer()->GetRealWeapon(Type);
	m_Pos = Pos;
	m_LastResetPos = Pos;
	m_Owner = Owner;
	m_HideOnSpec = HideOnSpec;
	m_LastResetTick = Server()->Tick();
	m_CalculatedVel = false;

	GameWorld()->InsertEntity(this);
}

void CStableProjectile::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CStableProjectile::TickDefered()
{
	if(Server()->Tick()%4 == 1)
	{
		m_LastResetPos = m_Pos;
		m_LastResetTick = Server()->Tick();
	}
	m_CalculatedVel = false;
}

void CStableProjectile::CalculateVel()
{
	float Time = (Server()->Tick()-m_LastResetTick)/(float)Server()->TickSpeed();
	float Curvature = 0;
	float Speed = 0;

	int TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	switch(m_Type)
	{
		case WEAPON_GRENADE:
			if (!TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
				Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[TuneZone].m_GrenadeCurvature;
				Speed = GameServer()->TuningList()[TuneZone].m_GrenadeSpeed;
			}
			break;

		case WEAPON_SHOTGUN:
			if (!TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
				Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[TuneZone].m_ShotgunCurvature;
				Speed = GameServer()->TuningList()[TuneZone].m_ShotgunSpeed;
			}
			break;

		case WEAPON_GUN:
			if (!TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_GunCurvature;
				Speed = GameServer()->Tuning()->m_GunSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[TuneZone].m_GunCurvature;
				Speed = GameServer()->TuningList()[TuneZone].m_GunSpeed;
			}
			break;
	}

	m_VelX = ((m_Pos.x - m_LastResetPos.x)/Time/Speed) * 100;
	m_VelY = ((m_Pos.y - m_LastResetPos.y)/Time/Speed - Time*Speed*Curvature/10000) * 100;

	m_CalculatedVel = true;
}

void CStableProjectile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
	{
		Reset();
		return;
	}

	CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (pOwner && pSnapChar)
	{
		int64_t TeamMask = pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner);
		if (!CmaskIsSet(TeamMask, SnappingClient))
			return;
	}

	if (m_HideOnSpec && pOwner && pOwner->IsPaused())
		return;


	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(!pProj)
		return;

	if(!m_CalculatedVel)
		CalculateVel();

	pProj->m_X = (int)m_LastResetPos.x;
	pProj->m_Y = (int)m_LastResetPos.y;
	pProj->m_VelX = m_VelX;
	pProj->m_VelY = m_VelY;
	pProj->m_StartTick = m_LastResetTick;
	pProj->m_Type = m_Type;
}
