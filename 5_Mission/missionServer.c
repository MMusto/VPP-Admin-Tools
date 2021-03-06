modded class MissionServer
{
	private bool m_ServerLocked = false;
	
	void MissionServer()
	{
		//=============RPC's====================
		GetRPCManager().AddRPC( "RPC_MissionServer", "RequestLockServer", this, SingeplayerExecutionType.Server );
		GetRPCManager().AddRPC( "RPC_MissionServer", "HandleChatCommand", this, SingeplayerExecutionType.Server );		
		//======================================
	}
	
	void RequestLockServer(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		if ( type == CallType.Server )
		{
			if (!GetPermissionManager().VerifyPermission(sender.GetPlainId(), "ServerManager:LockServer")) return;
			
			if (m_ServerLocked)
			{
				m_ServerLocked = false;
				GetPermissionManager().NotifyPlayer(sender.GetPlainId(),"Server is NO LONGER locked!",NotifyTypes.NOTIFY);
			}else{
				m_ServerLocked = true;
				GetPermissionManager().NotifyPlayer(sender.GetPlainId(),"Server is now locked!",NotifyTypes.NOTIFY);
			}
		}
	}
	
	void HandleChatCommand( CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
	{
		Param1<string> data; //chat string
        if ( !ctx.Read( data ) ) return;

        if (type == CallType.Server)
        {
			GetChatCommandManager().ParseCommand(data.param1,sender.GetPlainId());
		}
	}
	
	override void OnEvent(EventType eventTypeId, Param params) 
	{
		if (eventTypeId == ClientPrepareEventTypeID)
		{
			super.OnEvent(eventTypeId,params);
			
			ClientPrepareEventParams clientPrepareParams;
			Class.CastTo(clientPrepareParams, params);
			
			autoptr BannedPlayer bannedPlayer = GetBansManager().GetBannedPlayer(clientPrepareParams.param1.GetPlainId());
			if (bannedPlayer != null)
			{
				autoptr BanDuration expireDate = bannedPlayer.expirationDate;
				string banReason = bannedPlayer.banReason;
				if (expireDate.Permanent)
				{
					banReason += "\n Expiration Date: Permanent";
				}else{
					banReason += string.Format("\n Expiration Date %1/%2/%3  %4%5%6",expireDate.Year.ToString(),expireDate.Month.ToString(),expireDate.Day.ToString(),expireDate.Hour.ToString(),":",expireDate.Minute.ToString());
				}
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.InvokeKickPlayer, 3000, false, clientPrepareParams.param1,banReason); 
			}
			
			if (m_ServerLocked)
			{
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.InvokeKickPlayer, 3000, false, clientPrepareParams.param1,"Server is Currently Locked!");
			}
		}else{
			super.OnEvent(eventTypeId,params);
		}
	}
	
	void InvokeKickPlayer(PlayerIdentity identity, string msg)
	{
		if(identity != null)
			GetRPCManager().SendRPC( "RPC_MissionGameplay", "KickClientHandle", new Param1<string>( msg ), true, identity);
	}
		
	override void OnClientReconnectEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReconnectEvent(identity, player);
		if ( identity != null )
		{
			if(!GetPlayerListManager().HasPlayerInList(identity.GetPlainId()))
			{
				GetPlayerListManager().AddUserServer(identity.GetName(), identity.GetPlainId());
			}
			
			if(GetPermissionManager().HasUserGroup(identity.GetPlainId()))
			{
				GetPlayerListManager().SendPlayerList(identity);
			}
		}
	}
	
	override void UpdateLogoutPlayers()
	{
		for ( int i = 0; i < m_LogoutPlayers.Count(); i++ )
		{
			LogoutInfo info = m_LogoutPlayers.GetElement(i);
			
			if (GetGame().GetTime() >= info.param1)
			{
				PlayerIdentity identity;
				PlayerBase player = m_LogoutPlayers.GetKey(i);
				if (player)
				{
					identity = player.GetIdentity();
					if (identity != null)
					{
						if (GetPlayerListManager().HasPlayerInList(identity.GetPlainId()))
						{
							GetPlayerListManager().RemoveUserServer(identity.GetPlainId());
							Print("UpdateLogoutPlayers:: Updating VPP Sync List!");
						}
					}
					super.UpdateLogoutPlayers();
				}
			}
		}
	}
	
	/*
		overriding this function is a fucking BITCH TO DO. so i just copy pasta and add our shit to it. Fuck it.
	*/
	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		bool disconnectNow = true;
		
		// TODO: get out of vehicle
		// using database and no saving if authorization failed
		if (GetHive() && !authFailed)
		{			
			if (player.IsAlive())
			{	
				if (!m_LogoutPlayers.Contains(player))
				{
					Print("[Logout]: New player " + identity.GetPlainId() + " with logout time " + logoutTime.ToString());
					
					// inform client about logout time
					GetGame().SendLogoutTime(player, logoutTime);
			
					// wait for some time before logout and save
					LogoutInfo params = new LogoutInfo(GetGame().GetTime() + logoutTime * 1000, identity.GetPlainId());
					m_LogoutPlayers.Insert(player, params);
										
					// wait until logout timer runs out
					disconnectNow = false;		
				}
				return;
			}		
		}
		
		if (disconnectNow)
		{
			Print("[Logout]: New player " + identity.GetPlainId() + " with instant logout");
			
			// inform client about instant logout
			GetGame().SendLogoutTime(player, 0);
			
			PlayerDisconnected(player, identity, identity.GetPlainId());
		}
	}
	

	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		if (identity != null) uid = identity.GetPlainId();
		
		if (GetPlayerListManager().HasPlayerInList(uid))
		{
			GetPlayerListManager().RemoveUserServer(uid);
			Print("PlayerDisconnected:: Updating VPP Sync List! removed: "+uid);
		}
		super.PlayerDisconnected(player,identity,uid);
	}
}