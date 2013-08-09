 /* libIntegra multimedia module interface
 *  
 * Copyright (C) 2007 Birmingham City University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, 
 * USA.
 */

#include "platform_specifics.h"

#include "script_logic.h"


namespace ntg_internal
{
	CScriptLogic::CScriptLogic( const CNode &node )
		:	CLogic( node )
	{
	}


	CScriptLogic::~CScriptLogic()
	{
	}

	
	void CScriptLogic::handle_new( CServer &server, ntg_command_source source )
	{
		CLogic::handle_new( server, source );

		//todo - implement
	}


	void CScriptLogic::handle_set( CServer &server, const CNodeEndpoint &node_endpoint, const CValue *previous_value, ntg_command_source source )
	{
		CLogic::handle_set( server, node_endpoint, previous_value, source );

		//todo - implement
	}


	void CScriptLogic::handle_rename( CServer &server, const string &previous_name, ntg_command_source source )
	{
		CLogic::handle_rename( server, previous_name, source );

		//todo - implement
	}


	void CScriptLogic::handle_move( CServer &server, const CPath &previous_path, ntg_command_source source )
	{
		CLogic::handle_move( server, previous_path, source );

		//todo - implement
	}


	void CScriptLogic::handle_delete( CServer &server, ntg_command_source source )
	{
		CLogic::handle_delete( server, source );

		//todo - implement
	}


}
