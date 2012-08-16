/* 
 * Copyright (c) 2005 Zeus Technology
 *
 * See 
 * http://knowledgehub.zeus.com/faqs/2005/12/02/preserving_the_client_ip_address_to_apac 
 * for documentation and source for other versions of Apache.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Zeus Technology nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  mod_zeus.c -- Apache module to understand the X-Cluster-Client-Ip
 *                header, and feed it into the remote_ip field
 *
 * Editing history
 *   2005-12-02: Zeus Technology.  Initial version
 *
 * 
 * To use this module, first compile it into a DSO file and install
 * it into Apache's libexec directory by running:
 *
 *    $ apxs -c mod_zeus.c
 *    $ sudo apxs -i mod_zeus.so
 *
 * Then activate it in Apache's apache.conf file
 *
 *    #   apache.conf
 *    LoadModule zeus_module libexec/mod_zeus.so
 *    ZeusEnable On
 *    ZeusLoadBalancerIp 10.100.3.23 10.100.3.24
 *
 * Note, this module should be listed after all other modules in the
 * configuration file. This ensures that it is the first module to be run.
 *  
 * Then after restarting Apache via
 *
 *    $ sudo apachectl restart
 *
 * Apache will now fix the remote_ip field if the request contains an
 * X-Cluster-Client-Ip field, and the request came directly from a
 * one of the IP Addresses specified in the configuration file
 * (ZeusLoadBalancerIp directive).
 *
 * In addition, the environment variable ZEUS_LOAD_BALANCER_ADDR
 * will point to the IP Address of the Load Balancer machine that
 * the request came through.
 */ 

#include "httpd.h"
#include "http_config.h"
#include "http_log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

module MODULE_VAR_EXPORT zeus_module;

/* The structure that contains the configuration for this module */
typedef struct {
   int enabled;
   table * load_balancer_ips;
} mod_zeus_server_cfg;


/* Create a new (empty) server configuration structure */
static void *
mod_zeus_create_server_cfg( pool *p, server_rec *s )
{
   mod_zeus_server_cfg *cfg = (mod_zeus_server_cfg *)
      ap_pcalloc( p, sizeof( mod_zeus_server_cfg ));

   cfg->enabled = 0;
   cfg->load_balancer_ips = ap_make_table( p, 5 );

   return (void*) cfg;
}

/* Set the enabled flag for the module */
static const char*
mod_zeus_conf_enable( cmd_parms *cmd, void *dummy, int flag )
{
   server_rec *s = cmd->server;
   mod_zeus_server_cfg *cfg = (mod_zeus_server_cfg *)
      ap_get_module_config(s->module_config,  &zeus_module);

   cfg->enabled = flag;
   return NULL;
}

/* Add a load balancer IP Address to the allowable ip addresses */
static const char*
mod_zeus_conf_load_balancer_ip( cmd_parms *cmd, void *dummy, char *ip )
{
   struct in_addr addr;
   server_rec *s = cmd->server;
   mod_zeus_server_cfg *cfg = (mod_zeus_server_cfg *)
      ap_get_module_config(s->module_config,  &zeus_module);

   if( inet_aton( ip, &addr ) == 0 ) {
      return "Invalid IP Address";
   }
   ap_table_add( cfg->load_balancer_ips, ap_pstrdup( cmd->pool, ip ), "1" );
   return NULL;
}

/* Check to see if the headers contain an X-Cluster-Client-Ip
 * header (which is inserted by Zeus Load Balancer), and if it
 * does, feed it into the conn->remote_ip field.
 *
 * Also clear the remote_host field, so that DNS resolving works
 * using the correct IP adddress.
 *
 * Note, we always return DECLINED to the Apache core, so that it
 * carries on and performs any other actions that are defined on
 * this hook.
 */
static int
cluster_client_ip_hook (request_rec *r)
{
   mod_zeus_server_cfg *cfg;
   const char *ip;
   static char * real_remote_addr = 0;
   struct in_addr inp;

   /* Dont worry about internal redirects, we don't need to 
    * handle these, as the information has already been copied
    * from the main request */
   if( r->prev ) return DECLINED;
   
   /* Get the configuration */
   cfg = ap_get_module_config(r->server->module_config, &zeus_module);

   /* Check that the module is enabled */
   if( !cfg || !cfg->enabled ) return DECLINED;

   /* Get the Cluster Client Ip from the headers */
   ip = ap_table_get (r->headers_in, "X-Cluster-Client-Ip");
   if( !ip ) return DECLINED;

   /* Get the real connection client address (i.e. the one from the
    * load balancer */
   if( !real_remote_addr || r->connection->keepalives == 0 ) {
      /* New connection, remember the original remote host, so
       * we can inject it to later keepalive requests on the
       * same connection, there is no need to copy the memory,
       * as it is held in r->connection->pool */
      real_remote_addr = r->connection->remote_ip;
   }

   /* Check that this is a valid Load Balancer IP Address */
   if( ap_table_get( cfg->load_balancer_ips, real_remote_addr ) == 0 ) {
      ap_log_rerror( APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
                     "Ignoring X-Cluster-Client-Ip from "
                     "non-Load Balancer machine '%s'",
                     real_remote_addr );
      return DECLINED;
   }

   /* Check that the IP address is valid */
   if( inet_aton (ip, &inp ) == 0 ) {
      ap_log_rerror( APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, r,
                     "Invalid X-Cluster-Client-Ip header '%s'; ignored", ip );
      return DECLINED;
   }
   /* If it is valid, fill it in, and make side-effects */
   r->connection->remote_addr.sin_addr = inp;

   /* Preserve the Load Balancer Address */
   ap_table_set( r->subprocess_env, "ZEUS_LOAD_BALANCER_ADDR", 
                 real_remote_addr );

   /* Update the actual IP address (only updating it if it has changed to
    * avoid unnecessary DNS lookups for keepalive requests) */
   if( strcmp( r->connection->remote_ip, ip ) != 0 ) {
      r->connection->remote_ip   = ap_pstrdup( r->connection->pool, ip );
      r->connection->remote_host = NULL; /* Force DNS re-resolution */
   }

   return DECLINED;
}

/* Configuration parameters for the configuration file */
static command_rec mod_zeus_cmds[] = {
   { "ZeusEnable", mod_zeus_conf_enable, NULL,
     RSRC_CONF, FLAG, "Enable mod_zeus (preservation of Client IP Address "
     "through Zeus Load Balancer)" },

   { "ZeusLoadBalancerIP", mod_zeus_conf_load_balancer_ip, NULL,
     RSRC_CONF, ITERATE, "IP(s) of Zeus Load Balancers setting the "
     "X-Cluster-Client-Ip header" },
   { NULL }
};

/* Dispatch list for API hooks */
module MODULE_VAR_EXPORT zeus_module = {
    STANDARD_MODULE_STUFF, 
    NULL,                  /* module initializer                  */
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    mod_zeus_create_server_cfg,  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    mod_zeus_cmds,         /* table of config file commands       */
    NULL,                  /* [#8] MIME-typed-dispatched handlers */
    NULL,                  /* [#1] URI to filename translation    */
    NULL,                  /* [#4] validate user id from request  */
    NULL,                  /* [#5] check if the user is ok _here_ */
    NULL,                  /* [#3] check access by host address   */
    NULL,                  /* [#6] determine MIME type            */
    NULL,                  /* [#7] pre-run fixups                 */
    NULL,                  /* [#9] log a transaction              */
    NULL,                  /* [#2] header parser                  */
    NULL,                  /* child_init                          */
    NULL,                  /* child_exit                          */
    cluster_client_ip_hook /* [#0] post read-request              */
#ifdef EAPI
   ,NULL,                  /* EAPI: add_module                    */
    NULL,                  /* EAPI: remove_module                 */
    NULL,                  /* EAPI: rewrite_command               */
    NULL                   /* EAPI: new_connection                */
#endif
};

