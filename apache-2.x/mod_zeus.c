/* 
 * Copyright (c) 2005-2009 Zeus Technology
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
 *   2009-01-19: Zeus Technology.  Fix IP handling for IPv4 addresses
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
 *    # apache.conf
 *    LoadModule zeus_module libexec/mod_zeus.so
 *    ZeusEnable On
 *    ZeusLoadBalancerIP 10.100.3.23 10.100.3.24
 *
 * Note, this module should be listed after all other modules in the
 * configuration file. This ensures that it is the first module to be run.
 *
 * You can also use 'ZeusLoadBalancerIP *' to trust all upstream clients,
 * but should only do this if you are aware of the security consequences.
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

#include "apr_strings.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "util_script.h"
#include "http_main.h"
#include "http_request.h"

#include "mod_core.h"

typedef struct {
   int enabled;
   int trust_all;
   apr_table_t *trusted_ips;
} mod_zeus_server_conf;

module AP_MODULE_DECLARE_DATA zeus_module;

static void *create_zeus_config( apr_pool_t *p, server_rec *s )
{
   mod_zeus_server_conf *conf = apr_palloc( p, sizeof(*conf) );

   conf->enabled     = 0;
   conf->trust_all   = 0;
   conf->trusted_ips = apr_table_make(p, 10);

   return conf;
}

/* Enable or disable the module */
static const char *mod_zeus_conf_enable( cmd_parms *cmd, void *sconf_, int arg)
{
   server_rec *s = cmd->server;
   mod_zeus_server_conf *conf = (mod_zeus_server_conf *)
      ap_get_module_config( s->module_config, &zeus_module );

   conf->enabled = arg;
   return NULL;
}


/* Add a load balancer IP Address to the allowable ip addresses */
static const char*
mod_zeus_conf_load_balancer_ip( cmd_parms *cmd, void *sconf_, const char *ip )
{
   struct in_addr addr;

   server_rec *s = cmd->server;
   mod_zeus_server_conf *conf = (mod_zeus_server_conf *)
      ap_get_module_config( s->module_config, &zeus_module );

   if( strcmp( ip, "*" ) == 0 ) {
      conf->trust_all = 1;
      return NULL;
   }

   if( inet_aton( ip, &addr ) == 0 ) {
      return "Invalid IP Address";
   }
   apr_table_add( conf->trusted_ips, ip, "1" );
   return NULL;
}

/* Do we trust this connection?  
 * If it's the first time, check the source IP and cache the
 * result.
 * On subsequent times, the source IP may be invalid, but the cache 
 * will exist so we can use this value */
 
static int 
zeus_trusted( conn_rec *c, mod_zeus_server_conf *conf )
{
   const char *trusted;
   
   if( conf->trust_all ) return 1; /* YES */
 
   trusted = apr_table_get( c->notes, "ZEUS_TRUSTED" );
   if( trusted ) return ( trusted[0] == 'Y' );
   
   if( apr_table_get( conf->trusted_ips, c->remote_ip ) ) {
      /* is trusted */
      apr_table_setn( c->notes, "ZEUS_TRUSTED", "Y" );

      /* Preserve the Load Balancer Address */
      apr_table_set( c->notes, "ZEUS_LOAD_BALANCER_ADDR", c->remote_ip );
      return 1;
   } else {
      /* is not trusted */
      apr_table_setn( c->notes, "ZEUS_TRUSTED", "N" );
      return 0;
   }
}


static int 
zeus_handler(request_rec *r)
{
   mod_zeus_server_conf *conf = ap_get_module_config( r->server->module_config,
                                     &zeus_module );
   conn_rec *c = r->connection;
   const char* ip = 0;
   apr_sockaddr_t *inp;
    
   if( ! conf->enabled ) return DECLINED;
   
   /* Get the Cluster Client Ip from the headers */
   ip = apr_table_get( r->headers_in, "X-Cluster-Client-Ip" );
   if( !ip ) return DECLINED;

   if( !zeus_trusted( c, conf ) ) {
      ap_log_rerror( APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, r,
         "Ignoring X-Cluster-Client-Ip '%s' from non-Load Balancer machine '%s'",
         ip, c->remote_ip );
      return DECLINED;
   }

   /* Copy the true source address from the connection object,
    * so that it gets in the environment, and we can log it with 
    * %{ZEUS_LOAD_BALANCER_ADDR}e */
   apr_table_setn( r->subprocess_env, "ZEUS_LOAD_BALANCER_ADDR",
      apr_table_get( c->notes, "ZEUS_LOAD_BALANCER_ADDR" ) );

   /* If it's already correct, from an earlier KA for example,
    * do nothing */
   if( strcmp( ip, c->remote_ip ) == 0 ) return DECLINED;

   if( apr_sockaddr_info_get(&inp, ip, APR_UNSPEC, 
                             c->remote_addr->port, 0, c->pool) != APR_SUCCESS ) {
      ap_log_rerror( APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, r,
                     "Invalid X-Cluster-Client-Ip header '%s'; ignored", ip );
      return DECLINED;
   }

   c->remote_addr = inp;
   c->remote_ip   = apr_pstrdup( c->pool, ip );
   c->remote_host = NULL; /* Force DNS re-resolution */

   return DECLINED;
}

command_rec zeus_module_cmds[] = {
   AP_INIT_FLAG( "ZeusEnable", mod_zeus_conf_enable, NULL,
      RSRC_CONF, "Enable mod_zeus (preservation of Client IP Address through Zeus Load Balancer)" ),
   AP_INIT_ITERATE( "ZeusLoadBalancerIP", mod_zeus_conf_load_balancer_ip, NULL,
      RSRC_CONF, "IP(s) of Zeus Load Balancers setting the X-Cluster-Client-Ip header" ),
   { NULL }
};

static void register_hooks(apr_pool_t *p)
{
    ap_hook_post_read_request( zeus_handler, NULL, NULL, APR_HOOK_FIRST );
}

module AP_MODULE_DECLARE_DATA zeus_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,			/* create per-directory config structure */
    NULL,			/* merge per-directory config structures */
    create_zeus_config,	/* create per-server config structure */
    NULL,			/* merge per-server config structures */
    zeus_module_cmds,	/* command apr_table_t */
    register_hooks		/* register hooks */
};
