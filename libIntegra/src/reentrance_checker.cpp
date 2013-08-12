/** libIntegra multimedia module interface
 *
 * Copyright (C) 2012 Birmingham City University
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

#include <assert.h>

#include "reentrance_checker.h"
#include "trace.h"


namespace ntg_internal
{
	CReentranceChecker::CReentranceChecker()
	{
	}


	CReentranceChecker::~CReentranceChecker()
	{
	}


	bool CReentranceChecker::push( const CNodeEndpoint *node_endpoint, ntg_command_source source )
	{
		if( cares_about_source( source ) )
		{
			map_node_endpoint_to_source::const_iterator lookup = m_map_endpoint_to_source.find( node_endpoint );
			if( lookup != m_map_endpoint_to_source.end() )
			{
				if( cares_about_source( lookup->second ) )
				{
					return true;
				}
			}
		}

		m_map_endpoint_to_source[ node_endpoint ] = source;
		m_stack.push_back( node_endpoint );

		return false;
	}


	void CReentranceChecker::pop()
	{
		if( m_stack.empty() )
		{
			NTG_TRACE_ERROR << "attempt to pop empty queue";
			return;
		}

		m_map_endpoint_to_source.erase( m_stack.back() );
		m_stack.pop_back();
	}


	bool CReentranceChecker::cares_about_source( ntg_command_source source )
	{
		switch( source )
		{
			case NTG_SOURCE_SYSTEM:
			case NTG_SOURCE_CONNECTION:
			case NTG_SOURCE_SCRIPT:
				return true;	/* these are potential sources of recursion */

			case NTG_SOURCE_INITIALIZATION:
			case NTG_SOURCE_LOAD:
			case NTG_SOURCE_HOST:
			case NTG_SOURCE_XMLRPC_API:
			case NTG_SOURCE_C_API:
				return false;	/* these cannot cause recursion */

			default:
				assert( false );
				return false;
		}
	}
}
