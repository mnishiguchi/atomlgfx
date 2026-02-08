#ifndef __LGFX_PORT_H__
#define __LGFX_PORT_H__

#include <context.h>
#include <globalcontext.h>
#include <term.h>

#ifdef __cplusplus
extern "C" {
#endif

void lgfx_port_init(GlobalContext *global);
void lgfx_port_destroy(GlobalContext *global);
Context *lgfx_port_create_port(GlobalContext *global, term opts);

#ifdef __cplusplus
}
#endif

#endif
