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

#include "file_io.h"
#include "node.h"
#include "validator.h"
#include "module_manager.h"
#include "trace.h"
#include "server.h"
#include "api/command_api.h"
#include "api/command_result.h"
#include "data_directory.h"
#include "string_helper.h"

#include <assert.h>

#ifndef _WINDOWS
#define _S_IFMT S_IFMT
#endif


namespace ntg_internal
{
	const string CFileIO::s_file_suffix = "integra";

	/* 
	 we use linux-style path separators for all builds, because windows can use the two interchangeably, 
	 PD can't cope with windows separators at all, and zip files maintain the directionality of slashes 
	 (if we used system-specific slashes in zip files, the files would not be platform-independant)
	*/
	const char CFileIO::s_path_separator = '/';

	const int CFileIO::s_data_copy_buffer_size = 16384;

	const string CFileIO::s_data_directory_name = "integra_data/";

	const string CFileIO::s_internal_ixd_file_name = "integra_data/nodes.ixd";

	const string CFileIO::s_implementation_directory_name = "integra_data/implementation/";

	const string CFileIO::s_xml_encoding = "ISO-8859-1";

	const string CFileIO::s_integra_collection = "IntegraCollection";
	const string CFileIO::s_integra_version = "integraVersion";
	const string CFileIO::s_object = "object";
	const string CFileIO::s_attribute = "attribute";
	const string CFileIO::s_module_id = "moduleId";
	const string CFileIO::s_origin_id = "originId";
	const string CFileIO::s_name = "name";
	const string CFileIO::s_type_code = "typeCode";

	//used in older versions
	const string CFileIO::s_instance_id = "instanceId";
	const string CFileIO::s_class_id = "classId";



	CError CFileIO::load( CServer &server, const string &filename, const CNode *parent, guid_set &new_embedded_module_ids )
	{
		unsigned char *ixd_buffer = NULL;
		bool is_zip_file;
		unsigned int ixd_buffer_length;
		xmlTextReaderPtr reader = NULL;
		node_list new_nodes;
		node_list::const_iterator new_node_iterator;
		CValidator validator;

		LIBXML_TEST_VERSION;

		CModuleManager &module_manager = server.get_module_manager_writable();

		CError error = module_manager.load_from_integra_file( filename, new_embedded_module_ids );
		if( error != CError::SUCCESS ) 
		{
			NTG_TRACE_ERROR << "couldn't load modules: " << filename;
			goto CLEANUP;
		}

		/* pull ixd data out of file */
		error = load_ixd_buffer( filename, &ixd_buffer, &ixd_buffer_length, &is_zip_file );
		if( error != CError::SUCCESS ) 
		{
			NTG_TRACE_ERROR << "couldn't load ixd: " << filename;
			goto CLEANUP;
		}

		xmlInitParser();

		/* validate candidate IXD file against schema */
		error = validator.validate_ixd( (char *)ixd_buffer, ixd_buffer_length );
		if( error != CError::SUCCESS ) 
		{
			NTG_TRACE_ERROR << "ixd validation failed: " << filename;
			goto CLEANUP;
		}

		/* create ixd reader */
		reader = xmlReaderForMemory( (char *)ixd_buffer, ixd_buffer_length, NULL, NULL, 0 );
		if( reader == NULL )
		{
			NTG_TRACE_ERROR << "unable to read ixd: " << filename;
			error = CError::FAILED;
			goto CLEANUP;
		}

		/* stop DSP in the host */
		server.get_bridge()->host_dsp( 0 );

		/* actually load the data */
		error = load_nodes( server, parent, reader, new_nodes );
		if( error != CError::SUCCESS )
		{
			NTG_TRACE_ERROR << "failed to load nodes: " << filename;
			goto CLEANUP;
		}

		/* load the data directories */
		if( is_zip_file )
		{
			if( CDataDirectory::extract_from_zip( server, filename, parent ) != CError::SUCCESS )
			{
				NTG_TRACE_ERROR << "failed to load data directories: " << filename;
			}
		}

		/* send the loaded attributes to the host */
		for( new_node_iterator = new_nodes.begin(); new_node_iterator != new_nodes.end(); new_node_iterator++ )
		{
			if( send_loaded_values_to_host( **new_node_iterator, server.get_bridge() ) != CError::SUCCESS)
			{
				NTG_TRACE_ERROR << "failed to send loaded attributes to host: " << filename;
				continue;
			}
		}

		/* rename top-level node to filename */
		if( !new_nodes.empty() )
		{
			string top_level_node_name = get_top_level_node_name( filename );
			const CNode *top_level_node = *new_nodes.begin();

			if( top_level_node->get_name() != top_level_node_name )
			{
				server.process_command( CRenameCommandApi::create( top_level_node->get_path(), top_level_node_name.c_str() ), NTG_SOURCE_SYSTEM );
			}
		}


	CLEANUP:

		if( reader )
		{
			xmlFreeTextReader( reader );
		}

		if( ixd_buffer )
		{
			delete[] ixd_buffer;
		}

		if( error != CError::SUCCESS )
		{
			/* load failed - unload modules */
			module_manager.unload_modules( new_embedded_module_ids );
			new_embedded_module_ids.clear();
		}

		/* restart DSP in the host */
		server.get_bridge()->host_dsp( 1 );

		return error;
	}


	CError CFileIO::save( const CServer &server, const string &filename, const CNode &node )
	{
		zipFile zip_file;
		zip_fileinfo zip_file_info;
		unsigned char *ixd_buffer;
		unsigned int ixd_buffer_length;

		zip_file = zipOpen( filename.c_str(), APPEND_STATUS_CREATE );
		if( !zip_file )
		{
			NTG_TRACE_ERROR << "Failed to create zipfile: " << filename;
			return CError::FAILED;
		}

		if( save_nodes( server, node, &ixd_buffer, &ixd_buffer_length ) != CError::SUCCESS )
		{
			NTG_TRACE_ERROR << "Failed to save node tree: " << filename;
			return CError::FAILED;
		}

		init_zip_file_info( &zip_file_info );

		zipOpenNewFileInZip( zip_file, s_internal_ixd_file_name.c_str(), &zip_file_info, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION );
		zipWriteInFileInZip( zip_file, ixd_buffer, ixd_buffer_length );
		zipCloseFileInZip( zip_file );

		delete[] ixd_buffer;

		CDataDirectory::copy_to_zip( zip_file, node, node.get_parent_path() );

		copy_node_modules_to_zip( zip_file, node, server.get_module_manager() );

		zipClose( zip_file, NULL );

		return CError::SUCCESS;
	}


	void CFileIO::copy_file_to_zip( zipFile zip_file, const string &target_path, const string &source_path )
	{
		zip_fileinfo zip_file_info;
		init_zip_file_info( &zip_file_info );

		FILE *input_file = fopen( source_path.c_str(), "rb" );
		if( !input_file )
		{
			NTG_TRACE_ERROR << "couldn't open: " << source_path;
			return;
		}

		unsigned char *buffer = new unsigned char[ s_data_copy_buffer_size ];

		zipOpenNewFileInZip( zip_file, target_path.c_str(), &zip_file_info, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION );

		while( !feof( input_file ) )
		{
			size_t bytes_read = fread( buffer, 1, s_data_copy_buffer_size, input_file );
			if( bytes_read > 0 )
			{
				zipWriteInFileInZip( zip_file, buffer, bytes_read );
			}
			else
			{
				if( ferror( input_file ) )
				{
					NTG_TRACE_ERROR << "Error reading file: " << source_path;
					break;
				}
			}
		}
	
		zipCloseFileInZip( zip_file );

		delete[] buffer;

		fclose( input_file );
	}


	CError CFileIO::load_ixd_buffer( const string &file_path, unsigned char **ixd_buffer, unsigned int *ixd_buffer_length, bool *is_zip_file )
	{
		unzFile unzip_file;
		unz_file_info file_info;

		assert( ixd_buffer && ixd_buffer_length );

		unzip_file = unzOpen( file_path.c_str() );
		if( !unzip_file )
		{
			/* maybe file_path itself is an xml file saved before introduction of data directories */

			*is_zip_file = false;
			return load_ixd_buffer_directly( file_path, ixd_buffer, ixd_buffer_length );
		}
		else
		{
			*is_zip_file = true;
		}	

		if( unzLocateFile( unzip_file, s_internal_ixd_file_name.c_str(), 0 ) != UNZ_OK )
		{
			NTG_TRACE_ERROR << "Unable to locate " << s_internal_ixd_file_name << " in " << file_path;
			unzClose( unzip_file );
			return CError::FAILED;
		}

		if( unzGetCurrentFileInfo( unzip_file, &file_info, NULL, 0, NULL, 0, NULL, 0 ) != UNZ_OK )
		{
			NTG_TRACE_ERROR << "Couldn't get info for " << s_internal_ixd_file_name << " in " << file_path;
			unzClose( unzip_file );
			return CError::FAILED;
		}

		if( unzOpenCurrentFile( unzip_file ) != UNZ_OK )
		{
			NTG_TRACE_ERROR << "Unable to open " << s_internal_ixd_file_name << " in " << file_path;
			unzClose( unzip_file );
			return CError::FAILED;
		}

		*ixd_buffer_length = file_info.uncompressed_size;
		*ixd_buffer = new unsigned char[ *ixd_buffer_length ];

		if( unzReadCurrentFile( unzip_file, *ixd_buffer, *ixd_buffer_length ) != *ixd_buffer_length )
		{
			NTG_TRACE_ERROR << "Unable to read " << s_internal_ixd_file_name << " in " << file_path;
			unzClose( unzip_file );
			delete[] *ixd_buffer;
			return CError::FAILED;
		}

		unzClose( unzip_file );

		return CError::SUCCESS;
	}


	CError CFileIO::load_ixd_buffer_directly( const string &file_path, unsigned char **ixd_buffer, unsigned int *ixd_buffer_length )
	{
		FILE *file;
		size_t bytes_loaded;

		assert( ixd_buffer && ixd_buffer_length );
	
		file = fopen( file_path.c_str(), "rb" );
		if( !file )
		{
			NTG_TRACE_ERROR << "Couldn't open file: " << file_path;
			return CError::FAILED;
		}

		/* find size of the file */
		fseek( file, 0, SEEK_END );
		*ixd_buffer_length = ftell( file );
		fseek( file, 0, SEEK_SET );

		*ixd_buffer = new unsigned char[ *ixd_buffer_length ];
		bytes_loaded = fread( *ixd_buffer, 1, *ixd_buffer_length, file );
		fclose( file );

		if( bytes_loaded != *ixd_buffer_length )
		{
			NTG_TRACE_ERROR << "Error reading from file: " << file_path;
			return CError::FAILED;
		}

		return CError::SUCCESS;
	}


	CError CFileIO::load_nodes( CServer &server, const CNode *node, xmlTextReaderPtr reader, node_list &loaded_nodes )
	{
		xmlNodePtr          xml_node;
		xmlChar             *name;
		xmlChar             *content = NULL;
		unsigned int        depth;
		unsigned int        type;
		unsigned int        prev_depth;
		int                 rv;
		char				*saved_version;
		bool				saved_version_is_more_recent;
		const CNode *		parent = NULL;

		prev_depth      = 0;
		rv              = xmlTextReaderRead(reader);
	
		value_map loaded_values;

		if (!rv) 
		{
			return CError::INPUT_ERROR;
		}

		NTG_TRACE_VERBOSE << "loading... ";
		while( rv == 1 ) 
		{
			string element( ( const char * ) xmlTextReaderConstName( reader ) );
			depth = xmlTextReaderDepth(reader);
			type = xmlTextReaderNodeType(reader);

			if( element == s_integra_collection )
			{
				saved_version = ( char * ) xmlTextReaderGetAttribute( reader, BAD_CAST s_integra_version.c_str() );
				if( saved_version )
				{
					saved_version_is_more_recent = is_saved_version_newer_than_current( server, saved_version );
					xmlFree( saved_version );
					if( saved_version_is_more_recent )
					{
						return CError::FILE_MORE_RECENT_ERROR;
					}
				}
			}

			if( element == s_object ) 
			{
				if (depth > prev_depth) 
				{
					/* step down the node graph */
					parent = node;
				} 
				else
				{
					if (depth < prev_depth) 
					{
						/* step back up the node graph */
						assert( node->get_parent() );
						node = node->get_parent();
						parent = node->get_parent();
					} 
					else 
					{
						/* nesting level hasn't changed since last object */
						parent = node->get_parent();
					}
				}

				if( type == XML_READER_TYPE_ELEMENT ) 
				{
					const CInterfaceDefinition *interface_definition = find_interface( reader, server.get_module_manager() );
					if( interface_definition )
					{
						name = xmlTextReaderGetAttribute(reader, BAD_CAST s_name.c_str() );

						CPath empty_path;
						const CPath &parent_path = parent ? parent->get_path() : empty_path;
						/* add the new node */

						CNewCommandResult result;
						server.process_command( CNewCommandApi::create( interface_definition->get_module_guid(), (char * ) name, parent_path ), NTG_SOURCE_LOAD, &result );
						node = result.get_created_node();

						xmlFree(name);

						loaded_nodes.push_back( node );
					}
					else
					{
						NTG_TRACE_ERROR << "Can't find interface - skipping element";
					}
				}

				prev_depth = depth;
			}

			if( element == s_attribute ) 
			{
				if( type == XML_READER_TYPE_ELEMENT ) 
				{
					xml_node = xmlTextReaderExpand(reader);
					content = xmlNodeGetContent(xml_node);
					name = xmlTextReaderGetAttribute( reader, BAD_CAST s_name.c_str() );
					char *type_code_string = (char *)xmlTextReaderGetAttribute( reader, BAD_CAST s_type_code.c_str() );
					int type_code = atoi( type_code_string );
					xmlFree( type_code_string );

					CValue *value = CValue::factory( CValue::ixd_code_to_type( type_code ) );
					assert( value );

					if( content )
					{
						value->set_from_string( ( char * ) content );
						xmlFree( content );
						content = NULL;
					}

					const CNodeEndpoint *existing_node_endpoint = node->get_node_endpoint( ( char * ) name );
					if( existing_node_endpoint && existing_node_endpoint->get_endpoint_definition().should_load_from_ixd( value->get_type() ) )
					{
						/* 
						only store attribute if it exists and is of reasonable type 
						(could've been removed or changed from interface since ixd was written) 
						*/

						CPath path( node->get_path() );
						path.append_element( existing_node_endpoint->get_endpoint_definition().get_name() );

						loaded_values[ path.get_string() ] = value;
					}
					else
					{
						delete value;
					}

					xmlFree( name );
				}
			}

			rv = xmlTextReaderRead(reader);
		}

		NTG_TRACE_VERBOSE << "done!";

		NTG_TRACE_VERBOSE << "Setting values...";

		for( value_map::iterator value_iterator = loaded_values.begin(); value_iterator != loaded_values.end(); value_iterator++ )
		{
			CPath path( value_iterator->first );
			server.process_command( CSetCommandApi::create( path, value_iterator->second ), NTG_SOURCE_LOAD );
			delete value_iterator->second;
		}

		NTG_TRACE_VERBOSE << "done!";

		return CError::SUCCESS;
	}


	CError CFileIO::send_loaded_values_to_host( const CNode &node, ntg_bridge_interface *bridge )
	{
		const CInterfaceDefinition &interface_definition = node.get_interface_definition();

		if( !interface_definition.has_implementation() )
		{
			return CError::SUCCESS;
		}

		const endpoint_definition_list &endpoint_definitions = interface_definition.get_endpoint_definitions();
		for( endpoint_definition_list::const_iterator i = endpoint_definitions.begin(); i != endpoint_definitions.end(); i++ )
		{
			const CEndpointDefinition &endpoint_definition = **i;
			if( !endpoint_definition.should_send_to_host() || endpoint_definition.get_control_info()->get_type() != CControlInfo::STATEFUL )
			{
				continue;
			}

			const CNodeEndpoint *node_endpoint = node.get_node_endpoint( endpoint_definition.get_name() );
			assert( node_endpoint );

			bridge->send_value( node_endpoint );
		}

		return CError::SUCCESS;
	}


	string CFileIO::get_top_level_node_name( const string &filename )
	{
		int index_of_extension = 0;
		int i, length;

		/* strip path from filename */
		size_t last_slash = filename.find_last_of( '/' );
		size_t last_backslash = filename.find_last_of( '\\' );

		int index_after_last_slash = 0;
		int index_after_last_backslash = 0;

		if( last_slash != string::npos ) index_after_last_slash = ( last_slash + 1 );
		if( last_backslash != string::npos ) index_after_last_backslash = ( last_backslash + 1 );

		string name = filename.substr( MAX( index_after_last_slash, index_after_last_backslash ) );

		/* strip extension */
		index_of_extension = name.length() - s_file_suffix.length() - 1;
		if( index_of_extension > 0 && name.substr( index_of_extension ) == ( "." + s_file_suffix ) ) 
		{
			name = name.substr( 0, index_of_extension );
		}

		/* remove illegal characters */
		length = name.length();
		for( i = 0; i < length; i++ )
		{
			if( CStringHelper::s_node_name_character_set.find_first_of( name[ i ] ) == string::npos )
			{
				name[ i ] = '_';
			}
		}
	
		return name;
	}


	bool CFileIO::is_saved_version_newer_than_current( const CServer &server, const string &saved_version )
	{
		string current_version = server.get_libintegra_version();

		size_t last_dot_in_saved_version = saved_version.find_last_of( '.' );
		size_t last_dot_in_current_version = current_version.find_last_of( '.' );

		if( last_dot_in_saved_version == string::npos )
		{
			NTG_TRACE_ERROR << "Can't parse version string: " << saved_version;
			return false;
		}

		if( last_dot_in_current_version == string::npos )
		{
			NTG_TRACE_ERROR << "Can't parse version string: " << current_version;
			return false;
		}

		string saved_build_number = saved_version.substr( last_dot_in_saved_version + 1 );
		string current_build_number = current_version.substr( last_dot_in_current_version + 1 );

		return ( atoi( saved_build_number.c_str() ) > atoi( current_build_number.c_str() ) );
	}


	const CInterfaceDefinition *CFileIO::find_interface( xmlTextReaderPtr reader, const CModuleManager &module_manager )
	{
		/*
		 this method needs to deal with various permutations, due to the need to load modules by module id, origin id, and 
		 various old versions of integra files.

		 it's logic is as follows:

		 if element has a NTG_STR_MODULEID attribute, interpret this as the interface's module guid
	 
		 if element has a NTG_STR_ORIGINID attribute, interpret this as the interface's origin guid
		 else if element has a NTG_STR_INSTANCEID attribute, interpret this as the interface's origin guid
		 else if element has a NTG_STR_CLASSID attribute, interpret this attribute as a legacy numerical class id, from which 
											we can lookup the module's origin id using the ntg_interpret_legacy_module_id

		 if we have a module guid, and can find a matching module, use this module

		 else
			lookup the module using the origin guid
		*/

		GUID module_guid;
		GUID origin_guid;
		char *valuestr = NULL;

		assert( reader );

		module_guid = NULL_GUID;
		origin_guid = NULL_GUID;

		valuestr = (char *)xmlTextReaderGetAttribute(reader, BAD_CAST s_module_id.c_str() );
		if( valuestr )
		{
			CStringHelper::string_to_guid( valuestr, module_guid );
			xmlFree( valuestr );
		}

		valuestr = (char *)xmlTextReaderGetAttribute(reader, BAD_CAST s_origin_id.c_str() );
		if( valuestr )
		{
			CStringHelper::string_to_guid( valuestr, origin_guid );
			xmlFree( valuestr );
		}
		else
		{
			valuestr = (char *)xmlTextReaderGetAttribute(reader, BAD_CAST s_instance_id.c_str() );
			if( valuestr )
			{
				CStringHelper::string_to_guid( valuestr, origin_guid );
				xmlFree( valuestr );
			}
			else
			{
				valuestr = (char *) xmlTextReaderGetAttribute(reader, BAD_CAST s_class_id.c_str() );
				if( valuestr )
				{
					if( module_manager.interpret_legacy_module_id( atoi( valuestr ), origin_guid ) != CError::SUCCESS )
					{
						NTG_TRACE_ERROR << "Failed to interpret legacy class id: " << valuestr;
					}

					xmlFree( valuestr );
				}
			}
		}

		if( module_guid != NULL_GUID )
		{
			const CInterfaceDefinition *interface_definition = module_manager.get_interface_by_module_id( module_guid );
			if( interface_definition )
			{
				return interface_definition;
			}
		}

		if( origin_guid != NULL_GUID ) 
		{
			const CInterfaceDefinition *interface_definition = module_manager.get_interface_by_origin_id( origin_guid );
			return interface_definition;
		}

		return NULL;
	}








	CError CFileIO::save_nodes( const CServer &server, const CNode &node, unsigned char **buffer, unsigned int *buffer_length )
	{
		xmlTextWriterPtr writer;
		xmlBufferPtr write_buffer;
		int rc;

		assert( buffer && buffer_length );

		xmlInitParser();

		xmlSetBufferAllocationScheme( XML_BUFFER_ALLOC_DOUBLEIT );
		write_buffer = xmlBufferCreate();
		if( !write_buffer ) 
		{
			NTG_TRACE_ERROR << "error creating xml write buffer";
			return CError::FAILED;
		}

		writer = xmlNewTextWriterMemory( write_buffer, 0 );

		if( writer == NULL ) 
		{
			NTG_TRACE_ERROR << "Error creating the xml writer";
			return CError::FAILED;
		}

		xmlTextWriterSetIndent( writer, true );
		rc = xmlTextWriterStartDocument( writer, NULL, s_xml_encoding.c_str(), NULL );
		if (rc < 0) 
		{
			NTG_TRACE_ERROR << "Error at xmlTextWriterStartDocument";
			return CError::FAILED;
		}

		/* write header */
		xmlTextWriterStartElement(writer, BAD_CAST s_integra_collection.c_str() );
		xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:xsi", BAD_CAST "http://www.w3.org/2001/XMLSchema-node");

		string version_string = server.get_libintegra_version();
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST s_integra_version.c_str(), "%s", version_string.c_str() );

		if( save_node_tree( node, writer ) != CError::SUCCESS )
		{
			NTG_TRACE_ERROR << "Failed to save node";
			return CError::FAILED;
		}

		/* we don't strictly need this as xmlTextWriterEndDocument() tidies up */
		xmlTextWriterEndElement(writer);
		rc = xmlTextWriterEndDocument(writer);

		if (rc < 0) 
		{
			NTG_TRACE_ERROR << "Error at xmlTextWriterEndDocument";
			return CError::FAILED;
		}
		xmlFreeTextWriter(writer);

		*buffer = new unsigned char[ write_buffer->use ];
		memcpy( *buffer, write_buffer->content, write_buffer->use );
		*buffer_length = write_buffer->use;
		xmlBufferFree( write_buffer );

		return CError::SUCCESS;
	}


	void CFileIO::copy_node_modules_to_zip( zipFile zip_file, const CNode &node, const CModuleManager &module_manager )
	{
		assert( zip_file );

		guid_set module_guids_to_embed;
		find_module_guids_to_embed( node, module_guids_to_embed );

		for( guid_set::const_iterator i = module_guids_to_embed.begin(); i != module_guids_to_embed.end(); i++ )
		{
			const CInterfaceDefinition *interface_definition = module_manager.get_interface_by_module_id( *i );
			if( !interface_definition )
			{
				NTG_TRACE_ERROR << "Failed to retrieve interface";
				continue;
			}

			if( interface_definition->get_file_path().empty() )
			{
				NTG_TRACE_ERROR << "Failed to locate module file";
				continue;
			}

			string unique_interface_name = module_manager.get_unique_interface_name( *interface_definition );

			ostringstream target_path;
			target_path << s_implementation_directory_name << unique_interface_name << "." << CModuleManager::s_module_suffix;

			copy_file_to_zip( zip_file, target_path.str(), interface_definition->get_file_path() );
		}
	}


	CError CFileIO::save_node_tree( const CNode &node, xmlTextWriterPtr writer )
	{
		xmlChar *tmp;

		xmlTextWriterStartElement( writer, BAD_CAST s_object.c_str() );

		/* write out node->interface->module_guid */
		string guid_string = CStringHelper::guid_to_string( node.get_interface_definition().get_module_guid() );
		tmp = convert_input( guid_string, s_xml_encoding );
		xmlTextWriterWriteFormatAttribute(writer, BAD_CAST s_module_id.c_str(), (char * ) tmp );
		free( tmp );

		/* write out node->interface->origin_guid */
		guid_string = CStringHelper::guid_to_string( node.get_interface_definition().get_origin_guid() );
		tmp = convert_input( guid_string, s_xml_encoding );
		xmlTextWriterWriteFormatAttribute( writer, BAD_CAST s_origin_id.c_str(), (char * ) tmp );
		free( tmp );

		/* write out node->name */
		tmp = convert_input( node.get_name(), s_xml_encoding );
		xmlTextWriterWriteAttribute(writer, BAD_CAST s_name.c_str(), BAD_CAST tmp);
		free( tmp );

		const node_endpoint_map &node_endpoints = node.get_node_endpoints();
		for( node_endpoint_map::const_iterator node_endpoint_iterator = node_endpoints.begin(); node_endpoint_iterator != node_endpoints.end(); node_endpoint_iterator++ )
		{
			const CNodeEndpoint *node_endpoint = node_endpoint_iterator->second;
			const CValue *value = node_endpoint->get_value();
			const CEndpointDefinition &endpoint_definition = node_endpoint->get_endpoint_definition();
			if( !value || !endpoint_definition.get_control_info()->get_state_info()->get_is_saved_to_file() ) 
			{
				continue;
			}

			/* write attribute->name */
			CValue::type type = value->get_type();

			tmp = convert_input( endpoint_definition.get_name(), s_xml_encoding );
			xmlTextWriterStartElement(writer, BAD_CAST s_attribute.c_str() );
			xmlTextWriterWriteAttribute(writer, BAD_CAST s_name.c_str(), BAD_CAST tmp);
			free( tmp );

			/* write type */
			xmlTextWriterWriteFormatAttribute(writer, BAD_CAST s_type_code.c_str(), "%d", CValue::type_to_ixd_code( type ) );

			/* write attribute->value */
			string value_string = value->get_as_string();
			tmp = convert_input( value_string, s_xml_encoding );
			xmlTextWriterWriteString( writer, BAD_CAST tmp );
			xmlTextWriterEndElement( writer );
			free( tmp );
		}

		/* traverse children */
		const node_map &children = node.get_children();
		for( node_map::const_iterator i = children.begin(); i != children.end(); i++ )
		{
			save_node_tree( *i->second, writer );
		}

		xmlTextWriterEndElement( writer );
		return CError::SUCCESS;
	}


	void CFileIO::find_module_guids_to_embed( const CNode &node, guid_set &module_guids_to_embed )
	{
		if( node.get_interface_definition().should_embed() )
		{
			module_guids_to_embed.insert( node.get_interface_definition().get_module_guid() );
		}

		/* walk subtree */
		const node_map &children = node.get_children();
		for( node_map::const_iterator i = children.begin(); i != children.end(); i++ )
		{
			find_module_guids_to_embed( *i->second, module_guids_to_embed );
		}
	}



	void CFileIO::init_zip_file_info( zip_fileinfo *info )
	{
		time_t raw_time;
		struct tm *current_time;

		time( &raw_time );
		current_time = localtime( &raw_time );

		info->tmz_date.tm_year = current_time->tm_year;
		info->tmz_date.tm_mon = current_time->tm_mon;
		info->tmz_date.tm_mday = current_time->tm_mday;
		info->tmz_date.tm_hour = current_time->tm_hour;
		info->tmz_date.tm_min = current_time->tm_min;
		info->tmz_date.tm_sec = current_time->tm_sec;

		info->dosDate = 0;
		info->internal_fa = 0;
		info->external_fa = 0;
	}


	/* taken from libxml2 examples */
	xmlChar *CFileIO::convert_input( const string &in, const string &encoding )
	{
		unsigned char *out;
		int ret;
		int size;
		int out_size;
		int temp;
		xmlCharEncodingHandlerPtr handler;

		if( in.empty() )
		{
			return NULL;
		}

		handler = xmlFindCharEncodingHandler( encoding.c_str() );

		if (!handler) 
		{
			NTG_TRACE_ERROR << "ConvertInput: no encoding handler found for " << encoding;
			return NULL;
		}

		size = in.length() + 1;
		out_size = size * 2 - 1;
		out = new unsigned char[ out_size ];

		if (out != 0) 
		{
			temp = size - 1;
			ret = handler->input( out, &out_size, (const xmlChar *)in.c_str(), &temp );
			if ((ret < 0) || (temp - size + 1)) 
			{
				if (ret < 0) 
				{
					NTG_TRACE_ERROR << "ConvertInput: conversion wasn't successful.";
				} 
				else 
				{
					NTG_TRACE_ERROR << "ConvertInput: conversion wasn't successful. converted octets: " << temp;
				}

				free(out);
				out = NULL;
			} 
			else 
			{
				unsigned char *new_buffer = new unsigned char[ out_size + 1 ];
				memcpy( new_buffer, out, out_size );
				new_buffer[ out_size ] = 0;	/* null terminating out */
				delete out;
				out = new_buffer;
			}
		} 
		else 
		{
			NTG_TRACE_ERROR << "ConvertInput: no mem";
		}

		return out;
	}



}

