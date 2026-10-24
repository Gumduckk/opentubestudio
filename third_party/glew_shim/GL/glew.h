#pragma once
/*
 * Shim do GLEW para Linux (GLVND/Mesa).
 *
 * O renderer OpenGL do Cubism Framework inclui <GL/glew.h> quando compilado
 * com CSM_TARGET_LINUX_GL. Em vez de depender do GLEW (problemático no
 * Wayland), usamos as declarações padrão do sistema: com GL_GLEXT_PROTOTYPES,
 * GL/gl.h + GL/glext.h declaram todas as funções necessárias e elas são
 * resolvidas em tempo de execução pelo libOpenGL do GLVND — funciona tanto
 * no X11 quanto no Wayland.
 *
 * Extensões GLEW-específicas (ex.: GLEW_ARB_texture_barrier) ficam indefinidas
 * de propósito: o renderer cai no caminho de cópia de buffer, compatível com
 * qualquer driver.
 */
#ifndef __glew_h_shim__
#define __glew_h_shim__

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <GL/gl.h>
#include <GL/glext.h>

#endif
