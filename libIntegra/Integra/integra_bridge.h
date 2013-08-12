/* libIntegra multimedia module definition interface
 *  
 * Copyright (C) 2007 Jamie Bullock, Henrik Frisk
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

#ifndef INTEGRA_BRIDGE_H
#define INTEGRA_BRIDGE_H

/** \file integra_bridge.h Integra bridge API */


namespace integra_api
{
	class CValue;
}


namespace integra_internal
{
	class CNodeEndpoint;

	typedef unsigned long internal_id;
}



#ifdef _WIN32
	#ifdef INTEGRA_BRIDGE_EXPORTS	
		#define INTEGRA_BRIDGE_API __declspec(dllexport)
	#else
		#define INTEGRA_BRIDGE_API __declspec(dllimport)
	#endif
#else
	#define INTEGRA_BRIDGE_API 
#endif


/** \brief Interface giving functions to be supplied by an Integra bridge 
 *
 * Any compliant bridge, must implement all of these functions. However, it is
 * entirely dependent on the given module host how these functions are 
 * implemented, and it is expected that bridge implementations will vary 
 * considerably between module hosts. 
 *
 * Each bridge will need a mechanism for locating modules in the module host. 
 * This might involve creating a table that maps module ids to some local
 * identifier.
 *
 * */
typedef struct ntg_bridge_interface_ 
{

    /** \brief Load an Integra module in a target environment 
     * \param id: the unique session id of the module
     * \param *implementation_name: a pointer to a string giving a name
     that is meaningful to the module host
     *
     */
    int (*module_load)( integra_internal::internal_id id, const char *implementation_name);

    /** \brief Remove an Integra module from a target environment
     *
     * \param id: the unique session id of the module
     *
     */
    int (*module_remove)( integra_internal::internal_id id );

    /** \brief Connect two endpoints in a target environment
     *
     * \note { A bridge will only receive connect messages for non-message 
     * passing modules, e.g. audio and video modules. Message passing connects
     * are handled by libIntegra. }
     *
     */
    int (*module_connect)(const integra_internal::CNodeEndpoint *source, const integra_internal::CNodeEndpoint *target);

    /** \brief Disconnect endpoints in a target environment */
    int (*module_disconnect)(const integra_internal::CNodeEndpoint *source, const integra_internal::CNodeEndpoint *target);

    /** \brief send a value to a particular port */
    void (*send_value)(const integra_internal::CNodeEndpoint *target);

    /** \brief Initialise the bridge 
     *
     * */
    void (*bridge_init)(void);
    
    /** \brief Toggle DSP on or off
     *
     * \param status: 0 = off, 1 = on
     */
    void (*host_dsp)(int status);

    /** \brief Callback function that gets passed to the module host from the 
     * bridge when the server starts 
     *
     * \param void *data: Arbitrary environment-specific data
     *
     * A pointer can be returned to the module host via the return value of 
     * this function. Under normal circumstances the value of the pointer 
     * should be NULL
     *
     * */
    void (*bridge_callback)(int argc, void *data); 

    /** \brief Callback function that gets set by the server when the 
     * server starts
     *
     * This is the mechanism by which data gets passed from the bridge *back* 
     * to the server
     *
     * */
    void (*server_receive_callback)(integra_internal::internal_id id, const char *attribute_name, 
            const integra_api::CValue *value, void *context );

    /** \brief context for the server receive callback function
     *
     * */
	void *server_receive_callback_context;

} ntg_bridge_interface;

/** \brief function pointer for accessing a bridge interface */
extern const ntg_bridge_interface *(*ntg_interface_new)(void);

/** \brief Function to return a pointer to a bridge interface for use by the bridge host */
INTEGRA_BRIDGE_API const ntg_bridge_interface *ntg_bridge_interface_new(void);

typedef const ntg_bridge_interface *(*ntg_bridge_interface_generator)(void); 


#endif
