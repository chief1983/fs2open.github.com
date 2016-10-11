/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/


#include "gropengllight.h"
#include "gropenglshader.h"
#include "graphics/material.h"
#include "gropenglstate.h"
#include "math/vecmat.h"

extern GLfloat GL_max_anisotropy;


opengl_state GL_state;


opengl_texture_state::~opengl_texture_state()
{
	if (units != NULL) {
		vm_free(units);
	}
}

void opengl_texture_state::init(GLuint n_units)
{
	Assert( n_units > 0 );
	units = (opengl_texture_unit*) vm_malloc(n_units * sizeof(opengl_texture_unit));
	num_texture_units = n_units;

	for (unsigned int unit = 0; unit < num_texture_units; unit++) {
		units[unit].enabled = GL_FALSE;

		default_values(unit);

		glActiveTexture(GL_TEXTURE0 + unit);
	}

	SetActiveUnit();
}

void opengl_texture_state::default_values(GLint unit, GLenum target)
{
	glActiveTexture(GL_TEXTURE0 + unit);

	if (target == GL_INVALID_ENUM) {

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		units[unit].texture_target = GL_TEXTURE_2D;
		units[unit].texture_id = 0;
	}
}

void opengl_texture_state::SetTarget(GLenum tex_target)
{
	if (units[active_texture_unit].texture_target != tex_target) {

		if (units[active_texture_unit].texture_id) {
			glBindTexture(units[active_texture_unit].texture_target, 0);
			units[active_texture_unit].texture_id = 0;
		}

		// reset modes, since those were only valid for the previous texture target
		default_values(active_texture_unit, tex_target);
		units[active_texture_unit].texture_target = tex_target;
	}
}

void opengl_texture_state::SetActiveUnit(GLuint id)
{
	if (id >= num_texture_units) {
		Int3();
		id = 0;
	}

	glActiveTexture(GL_TEXTURE0 + id);

	active_texture_unit = id;
}

void opengl_texture_state::Enable(GLuint tex_id)
{
	if ( units[active_texture_unit].texture_id == tex_id ) {
		return;
	}

	if (units[active_texture_unit].texture_id != tex_id) {
		glBindTexture(units[active_texture_unit].texture_target, tex_id);
		units[active_texture_unit].texture_id = tex_id;
	}
}

void opengl_texture_state::Delete(GLuint tex_id)
{
	if (tex_id == 0) {
		Int3();
		return;
	}

	GLuint atu_save = active_texture_unit;

	for (unsigned int i = 0; i < num_texture_units; i++) {
		if (units[i].texture_id == tex_id) {
			SetActiveUnit(i);

			glBindTexture(units[i].texture_target, 0);
			units[i].texture_id = 0;

			default_values(i, units[i].texture_target);

			if (i == atu_save) {
				atu_save = 0;
			}
		}
	}

	SetActiveUnit(atu_save);
}

void opengl_state::init()
{
	int i;
	
	glDisable(GL_BLEND);
	blend_Status = GL_FALSE;

	glDisable(GL_DEPTH_TEST);
	depthtest_Status = GL_FALSE;

	glDisable(GL_SCISSOR_TEST);
	scissortest_Status = GL_FALSE;

	glDisable(GL_CULL_FACE);
	cullface_Status = GL_FALSE;

	glDisable(GL_POLYGON_OFFSET_FILL);
	polygonoffsetfill_Status = GL_FALSE;

	polygon_offset_Factor = 0.0f;
	polygon_offset_Unit = 0.0f;

	normalize_Status = GL_FALSE;

	for (i = 0; i < (int)(sizeof(clipplane_Status) / sizeof(GLboolean)); i++) {
		//glDisable(GL_CLIP_PLANE0+i);
		clipplane_Status[i] = GL_FALSE;
	}

	if (GL_version >= 30) {
		for (i = 0; i < (int)(sizeof(clipdistance_Status) / sizeof(GLboolean)); i++) {
			//glDisable(GL_CLIP_DISTANCE0+i);
			clipdistance_Status[i] = GL_FALSE;
		}
	}

	glDepthMask(GL_FALSE);
	depthmask_Status = GL_FALSE;

	glFrontFace(GL_CCW);
	frontface_Value = GL_CCW;

	glCullFace(GL_BACK);
	cullface_Value = GL_BACK;

	glBlendFunc(GL_ONE, GL_ZERO);
	blendfunc_Value[0] = GL_ONE;
	blendfunc_Value[1] = GL_ZERO;

	glDepthFunc(GL_LESS);
	depthfunc_Value = GL_LESS;

	glGetFloatv(GL_LINE_WIDTH, &line_width_Value);

	Current_alpha_blend_mode = ALPHA_BLEND_NONE;

	current_program = 0;
	glUseProgram(0);
}

GLboolean opengl_state::Blend(GLint state)
{
	GLboolean save_state = blend_Status;

	if ( !((state == -1) || (state == blend_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glEnable(GL_BLEND);
			blend_Status = GL_TRUE;
		} else {
			glDisable(GL_BLEND);
			blend_Status = GL_FALSE;
		}

		Current_alpha_blend_mode = (gr_alpha_blend)(-1);
	}

	return save_state;
}

GLboolean opengl_state::DepthTest(GLint state)
{
	GLboolean save_state = depthtest_Status;

	if ( !((state == -1) || (state == depthtest_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glEnable(GL_DEPTH_TEST);
			depthtest_Status = GL_TRUE;
		} else {
			glDisable(GL_DEPTH_TEST);
			depthtest_Status = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::ScissorTest(GLint state)
{
	GLboolean save_state = scissortest_Status;

	if ( !((state == -1) || (state == scissortest_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glEnable(GL_SCISSOR_TEST);
			scissortest_Status = GL_TRUE;
		} else {
			glDisable(GL_SCISSOR_TEST);
			scissortest_Status = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::StencilTest(GLint state)
{
    GLboolean save_state = stenciltest_Status;

    if ( !((state == -1) || (state == stenciltest_Status)) ) {
        if (state) {
            Assert( state == GL_TRUE );
            glEnable(GL_STENCIL_TEST);
            stenciltest_Status = GL_TRUE;
        } else {
            glDisable(GL_STENCIL_TEST);
            stenciltest_Status = GL_FALSE;
        }
    }

    return save_state;
}

GLboolean opengl_state::CullFace(GLint state)
{
	GLboolean save_state = cullface_Status;

	if ( !((state == -1) || (state == cullface_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glEnable(GL_CULL_FACE);
			cullface_Status = GL_TRUE;
		} else {
			glDisable(GL_CULL_FACE);
			cullface_Status = GL_FALSE;
		}
	}

	return save_state;
}

void opengl_state::SetPolygonMode(GLenum face, GLenum mode)
{
	if ( polygon_mode_Face != face || polygon_mode_Mode != mode ) {
		glPolygonMode(face, mode);

		polygon_mode_Face = face;
		polygon_mode_Mode = mode;
	}
}

void opengl_state::SetPolygonOffset(GLfloat factor, GLfloat units)
{
	if ( polygon_offset_Factor != factor || polygon_offset_Unit != units) {
		glPolygonOffset(factor, units);

		polygon_offset_Factor = factor;
		polygon_offset_Unit = units;
	}
}

GLboolean opengl_state::PolygonOffsetFill(GLint state)
{
	GLboolean save_state = polygonoffsetfill_Status;

	if ( !((state == -1) || (state == polygonoffsetfill_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glEnable(GL_POLYGON_OFFSET_FILL);
			polygonoffsetfill_Status = GL_TRUE;
		} else {
			glDisable(GL_POLYGON_OFFSET_FILL);
			polygonoffsetfill_Status = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::ClipPlane(GLint num, GLint state)
{
	Assert( (num >= 0) || (num < (int)(sizeof(clipplane_Status) / sizeof(GLboolean))) );

	GLboolean save_state = clipplane_Status[num];

	if ( !((state == -1) || (state == clipplane_Status[num])) ) {
		if (state) {
			Assert( state == GL_TRUE );
			clipplane_Status[num] = GL_TRUE;
		} else {
			clipplane_Status[num] = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::ClipDistance(GLint num, GLint state)
{
	Assert( (num >= 0) || (num < (int)(sizeof(clipdistance_Status) / sizeof(GLboolean))) || GL_version >= 30 );

	GLboolean save_state = clipdistance_Status[num];

	if ( !((state == -1) || (state == clipdistance_Status[num])) ) {
		if (state) {
			Assert( state == GL_TRUE );
			//glEnable(GL_CLIP_DISTANCE0+num);
			clipdistance_Status[num] = GL_TRUE;
		} else {
			//glDisable(GL_CLIP_DISTANCE0+num);
			clipdistance_Status[num] = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::DepthMask(GLint state)
{
	GLboolean save_state = depthmask_Status;

	if ( !((state == -1) || (state == depthmask_Status)) ) {
		if (state) {
			Assert( state == GL_TRUE );
			glDepthMask(GL_TRUE);
			depthmask_Status = GL_TRUE;
		} else {
			glDepthMask(GL_FALSE);
			depthmask_Status = GL_FALSE;
		}
	}

	return save_state;
}

GLboolean opengl_state::ColorMask(GLint state)
{
    GLboolean save_state = colormask_Status;

    if ( !((state == -1) || (state == colormask_Status)) ) {
        if (state) {
            Assert( state == GL_TRUE );
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            colormask_Status = GL_TRUE;
        } else {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            colormask_Status = GL_FALSE;
        }
    }

    return save_state;
}

void opengl_state::SetAlphaBlendMode(gr_alpha_blend ab)
{
	if (ab == Current_alpha_blend_mode) {
		return;
	}

	switch (ab) {
		case ALPHA_BLEND_ALPHA_BLEND_ALPHA:
			GL_state.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;

		case ALPHA_BLEND_NONE:
			GL_state.BlendFunc(GL_ONE, GL_ZERO);
			break;

		case ALPHA_BLEND_ADDITIVE:
			GL_state.BlendFunc(GL_ONE, GL_ONE);
			break;

		case ALPHA_BLEND_ALPHA_ADDITIVE:
			GL_state.BlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;

		case ALPHA_BLEND_ALPHA_BLEND_SRC_COLOR:
			GL_state.BlendFunc(/*GL_SRC_COLOR*/GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
			break;

		case ALPHA_BLEND_PREMULTIPLIED:
			GL_state.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;

		default:
			break;
	}

	GL_state.Blend( (ab == ALPHA_BLEND_NONE) ? GL_FALSE : GL_TRUE );

	Current_alpha_blend_mode = ab;
}

void opengl_state::SetZbufferType(gr_zbuffer_type zt)
{
	switch (zt) {
		case ZBUFFER_TYPE_NONE:
			GL_state.DepthFunc(GL_ALWAYS);
			GL_state.DepthMask(GL_FALSE);
			break;

		case ZBUFFER_TYPE_READ:
			GL_state.DepthFunc(GL_LESS);
			GL_state.DepthMask(GL_FALSE);
			break;

		case ZBUFFER_TYPE_WRITE:
			GL_state.DepthFunc(GL_ALWAYS);
			GL_state.DepthMask(GL_TRUE);
			break;

		case ZBUFFER_TYPE_FULL:
			GL_state.DepthFunc(GL_LESS);
			GL_state.DepthMask(GL_TRUE);
			break;

		default:
			break;
	}

	GL_state.DepthTest( (zt == ZBUFFER_TYPE_NONE) ? GL_FALSE : GL_TRUE );
}

void opengl_state::SetStencilType(gr_stencil_type st)
{
    if (st == Current_stencil_type) {
        return;
    }
    
    switch (st) {
        case STENCIL_TYPE_NONE:
            glStencilFunc( GL_NEVER, 1, 0xFFFF );
            glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
            break;
            
        case STENCIL_TYPE_READ:
            glStencilFunc( GL_NOTEQUAL, 1, 0XFFFF );
            glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
            break;
            
        case STENCIL_TYPE_WRITE:
            glStencilFunc( GL_ALWAYS, 1, 0xFFFF );
            glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
            break;
                     
        default:
            break;
    }
           
    GL_state.StencilTest( (st == STENCIL_TYPE_NONE) ? GL_FALSE : GL_TRUE );
         
    Current_stencil_type = st;
}

void opengl_state::SetLineWidth(GLfloat width)
{
	if ( width == line_width_Value ) {
		return;
	}

	glLineWidth(width);
	line_width_Value = width;
}
void opengl_state::UseProgram(GLuint program)
{
	if (current_program == program) {
		return;
	}

	current_program = program;
	glUseProgram(program);
}
bool opengl_state::IsCurrentProgram(GLuint program) {
	return current_program == program;
}

opengl_array_state::~opengl_array_state()
{
	if ( client_texture_units != NULL ) {
		vm_free(client_texture_units);
	}
}

void opengl_array_state::init(GLuint n_units)
{
	Assert( n_units > 0 );
	client_texture_units = (opengl_client_texture_unit*) vm_malloc(n_units * sizeof(opengl_client_texture_unit));
	num_client_texture_units = n_units;
	active_client_texture_unit = 0;

	for (unsigned int i = 0; i < num_client_texture_units; i++) {
		client_texture_units[i].pointer = 0;
		client_texture_units[i].size = 4;
		client_texture_units[i].status = GL_FALSE;
		client_texture_units[i].stride = 0;
		client_texture_units[i].type = GL_FLOAT;
		client_texture_units[i].buffer = 0;
		client_texture_units[i].reset_ptr = false;
		client_texture_units[i].used_for_draw = false;
	}

	color_array_Buffer = 0;
	color_array_Status = GL_FALSE;
	color_array_size = 4;
	color_array_type = GL_FLOAT;
	color_array_stride = 0;
	color_array_pointer = 0;
	color_array_reset_ptr = false;
	color_array_used_for_draw = false;

	normal_array_Buffer = 0;
	normal_array_Status = GL_FALSE;
	normal_array_Type = GL_FLOAT;
	normal_array_Stride = 0;
	normal_array_Pointer = 0;
	normal_array_reset_ptr = false;
	normal_array_used_for_draw = false;

	vertex_array_Buffer = 0;
	vertex_array_Status = GL_FALSE;
	vertex_array_Size = 4;
	vertex_array_Type = GL_FLOAT;
	vertex_array_Stride = 0;
	vertex_array_Pointer = 0;
	vertex_array_reset_ptr = false;
	vertex_array_used_for_draw = false;

	array_buffer = 0;
	element_array_buffer = 0;
	texture_array_buffer = 0;
	uniform_buffer = 0;
}

void opengl_array_state::SetActiveClientUnit(GLuint id)
{
	// this function is deprecated in OGL Core
}

void opengl_array_state::EnableClientTexture()
{
	client_texture_units[active_client_texture_unit].used_for_draw = true;

	if ( client_texture_units[active_client_texture_unit].status == GL_TRUE ) {
		return;
	}


	client_texture_units[active_client_texture_unit].status = GL_TRUE;
}

void opengl_array_state::DisableClientTexture()
{
	if ( client_texture_units[active_client_texture_unit].status == GL_FALSE ) {
		return;
	}


	client_texture_units[active_client_texture_unit].status = GL_FALSE;
}

void opengl_array_state::TexPointer(GLint size, GLenum type, GLsizei stride, GLvoid *pointer)
{
	opengl_client_texture_unit *ct_unit = &client_texture_units[active_client_texture_unit];

	if ( 
		!ct_unit->reset_ptr 
		&& ct_unit->pointer == pointer 
		&& ct_unit->size == size 
		&& ct_unit->type == type 
		&& ct_unit->stride == stride 
		&& ct_unit->buffer == array_buffer 
	) {
		return;
	}


	ct_unit->size = size;
	ct_unit->type = type;
	ct_unit->stride = stride;
	ct_unit->pointer = pointer;
	ct_unit->buffer = array_buffer;
	ct_unit->reset_ptr = false;
}

void opengl_array_state::EnableClientColor()
{
	color_array_used_for_draw = true;

	if ( color_array_Status == GL_TRUE ) {
		return;
	}


	color_array_Status = GL_TRUE;
}

void opengl_array_state::DisableClientColor()
{
	if ( color_array_Status == GL_FALSE ) {
		return;
	}


	color_array_Status = GL_FALSE;
}

void opengl_array_state::ColorPointer(GLint size, GLenum type, GLsizei stride, GLvoid *pointer)
{
	if ( 
		!color_array_reset_ptr 
		&& color_array_size == size 
		&& color_array_type == type 
		&& color_array_stride == stride 
		&& color_array_pointer == pointer 
		&& color_array_Buffer == array_buffer 
	) {
		return;
	}


	color_array_size = size;
	color_array_type = type;
	color_array_stride = stride;
	color_array_pointer = pointer;
	color_array_Buffer = array_buffer;
	color_array_reset_ptr = false;
}

void opengl_array_state::EnableClientNormal()
{
	normal_array_used_for_draw = true;

	if ( normal_array_Status == GL_TRUE ) {
		return;
	}


	normal_array_Status = GL_TRUE;
}

void opengl_array_state::DisableClientNormal()
{
	if ( normal_array_Status == GL_FALSE ) {
		return;
	}

	normal_array_Status = GL_FALSE;
}

void opengl_array_state::NormalPointer(GLenum type, GLsizei stride, GLvoid *pointer)
{
	if ( 
		!normal_array_reset_ptr 
		&& normal_array_Type == type 
		&& normal_array_Stride == stride 
		&& normal_array_Pointer == pointer 
		&& normal_array_Buffer == array_buffer 
	) {
		return;
	}

	normal_array_Type = type;
	normal_array_Stride = stride;
	normal_array_Pointer = pointer;
	normal_array_Buffer = array_buffer;
	normal_array_reset_ptr = false;
}

void opengl_array_state::EnableClientVertex()
{
	vertex_array_used_for_draw = true;

	if ( vertex_array_Status == GL_TRUE ) {
		return;
	}

	vertex_array_Status = GL_TRUE;
}

void opengl_array_state::DisableClientVertex()
{
	if ( vertex_array_Status == GL_FALSE ) {
		return;
	}

	vertex_array_Status = GL_FALSE;
}

void opengl_array_state::VertexPointer(GLint size, GLenum type, GLsizei stride, GLvoid *pointer)
{
	if (
		!vertex_array_reset_ptr 
		&& vertex_array_Size == size 
		&& vertex_array_Type == type 
		&& vertex_array_Stride == stride 
		&& vertex_array_Pointer == pointer 
		&& vertex_array_Buffer == array_buffer 
	) {
		return;
	}

	vertex_array_Size = size;
	vertex_array_Type = type;
	vertex_array_Stride = stride;
	vertex_array_Pointer = pointer;
	vertex_array_Buffer = array_buffer;
	vertex_array_reset_ptr = false;
}

void opengl_array_state::EnableVertexAttrib(GLuint index)
{
	opengl_vertex_attrib_unit *va_unit = &vertex_attrib_units[index];

	va_unit->used_for_draw = true;

	if ( va_unit->status_init && va_unit->status == GL_TRUE ) {
		return;
	}

	glEnableVertexAttribArray(index);
	va_unit->status = GL_TRUE;
	va_unit->status_init = true;
}

void opengl_array_state::DisableVertexAttrib(GLuint index)
{
	opengl_vertex_attrib_unit *va_unit = &vertex_attrib_units[index];

	if ( va_unit->status_init && va_unit->status == GL_FALSE ) {
		return;
	}

	glDisableVertexAttribArray(index);
	va_unit->status = GL_FALSE;
	va_unit->status_init = true;
}

void opengl_array_state::VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer)
{
	opengl_vertex_attrib_unit *va_unit = &vertex_attrib_units[index];

	if ( 
		!va_unit->reset_ptr 
		&& va_unit->ptr_init 
		&& va_unit->normalized == normalized 
		&& va_unit->pointer == pointer 
		&& va_unit->size == size 
		&& va_unit->stride == stride 
		&& va_unit->type == type 
		&& va_unit->buffer == array_buffer
	) {
		return;
	}

	glVertexAttribPointer(index, size, type, normalized, stride, pointer);

	va_unit->normalized = normalized;
	va_unit->pointer = pointer;
	va_unit->size = size;
	va_unit->stride = stride;
	va_unit->type = type;
	va_unit->buffer = array_buffer;
	va_unit->reset_ptr = false;

	va_unit->ptr_init = true;
}

void opengl_array_state::ResetVertexAttribs()
{
	SCP_map<GLuint, opengl_vertex_attrib_unit>::iterator it;

	for ( it = vertex_attrib_units.begin(); it != vertex_attrib_units.end(); ++it ) {
		DisableVertexAttrib(it->first);
	}

	vertex_attrib_units.clear();
}

void opengl_array_state::BindPointersBegin()
{
	// set all available client states to not used
	vertex_array_used_for_draw = false;
	color_array_used_for_draw = false;
	normal_array_used_for_draw = false;

	for (unsigned int i = 0; i < num_client_texture_units; i++) {
		client_texture_units[i].used_for_draw = false;
	}

	SCP_map<GLuint, opengl_vertex_attrib_unit>::iterator it;

	for (it = vertex_attrib_units.begin(); it != vertex_attrib_units.end(); ++it) {
		it->second.used_for_draw = false;
	}
}

void opengl_array_state::BindPointersEnd()
{
	// any client states not used, disable them
	if ( !vertex_array_used_for_draw ) DisableClientVertex();
	if ( !color_array_used_for_draw ) DisableClientColor();
	if ( !normal_array_used_for_draw ) DisableClientNormal();

	for (unsigned int i = 0; i < num_client_texture_units; i++) {
		if ( !client_texture_units[i].used_for_draw ) {
			SetActiveClientUnit(i);
			DisableClientTexture();
		}
	}

	SCP_map<GLuint, opengl_vertex_attrib_unit>::iterator it;

	for (it = vertex_attrib_units.begin(); it != vertex_attrib_units.end(); ++it) {
		if ( !it->second.used_for_draw ) DisableVertexAttrib(it->first);
	}
}

void opengl_array_state::BindArrayBuffer(GLuint id)
{
	if ( array_buffer == id ) {
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, id);

	array_buffer = id;

	vertex_array_reset_ptr = true;
	color_array_reset_ptr = true;
	normal_array_reset_ptr = true;

	for (unsigned int i = 0; i < num_client_texture_units; i++) {
		client_texture_units[i].reset_ptr = true;
	}

	SCP_map<GLuint,opengl_vertex_attrib_unit>::iterator it;

	for ( it = vertex_attrib_units.begin(); it != vertex_attrib_units.end(); ++it ) {
		it->second.reset_ptr = true;
	}
}

void opengl_array_state::BindElementBuffer(GLuint id)
{
	if ( element_array_buffer == id ) {
		return;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);

	element_array_buffer = id;
}

void opengl_array_state::BindTextureBuffer(GLuint id)
{
	if ( texture_array_buffer == id ) {
		return;
	}

	glBindBuffer(GL_TEXTURE_BUFFER, id);

	texture_array_buffer = id;
}

void opengl_array_state::BindUniformBufferBindingIndex(GLuint id, GLuint index)
{
	if ( uniform_buffer_index_bindings[index] == id ) {
		return;
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, index, id);

	uniform_buffer_index_bindings[index] = id;
}

void opengl_array_state::BindUniformBuffer(GLuint id)
{
	if ( uniform_buffer == id ) {
		return;
	}

	glBindBuffer(GL_UNIFORM_BUFFER, id);

	uniform_buffer = id;
}

opengl_light_state::opengl_light_state(int light_num): Light_num(light_num)
{

}

void opengl_light_state::Enable()
{
	Assert ( Light_num >= 0 && Light_num < GL_max_lights );

	if ( Enabled ) {
		return;
	}

	Enabled = true;
}

void opengl_light_state::Disable()
{
	Assert ( Light_num >= 0 && Light_num < GL_max_lights );

	if ( !Enabled ) {
		return;
	}

	Enabled = false;
}

void opengl_light_state::Invalidate()
{
	InvalidPosition = true;
}

void opengl_light_state::SetPosition(GLfloat *val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( !InvalidPosition && 
		fl_equal(Position[0], val[0]) && 
		fl_equal(Position[1], val[1]) && 
		fl_equal(Position[2], val[2]) && 
		fl_equal(Position[3], val[3]) ) {
		return;
	}

	Position[0] = val[0];
	Position[1] = val[1];
	Position[2] = val[2];
	Position[3] = val[3];

	InvalidPosition = false;
}

void opengl_light_state::SetAmbient(GLfloat *val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(Ambient[0], val[0]) &&
		fl_equal(Ambient[1], val[1]) &&
		fl_equal(Ambient[2], val[2]) &&
		fl_equal(Ambient[3], val[3]) ) {
		return;
	}

	Ambient[0] = val[0];
	Ambient[1] = val[1];
	Ambient[2] = val[2];
	Ambient[3] = val[3];
}

void opengl_light_state::SetDiffuse(GLfloat *val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(Diffuse[0], val[0]) &&
		fl_equal(Diffuse[1], val[1]) &&
		fl_equal(Diffuse[2], val[2]) &&
		fl_equal(Diffuse[3], val[3]) ) {
		return;
	}

	Diffuse[0] = val[0];
	Diffuse[1] = val[1];
	Diffuse[2] = val[2];
	Diffuse[3] = val[3];

}

void opengl_light_state::SetSpecular(GLfloat *val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(Specular[0], val[0]) &&
		fl_equal(Specular[1], val[1]) &&
		fl_equal(Specular[2], val[2]) &&
		fl_equal(Specular[3], val[3]) ) {
		return;
	}

	Specular[0] = val[0];
	Specular[1] = val[1];
	Specular[2] = val[2];
	Specular[3] = val[3];

}

void opengl_light_state::SetConstantAttenuation(GLfloat val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(ConstantAttenuation, val) ) {
		return;
	}

	ConstantAttenuation = val;

}

void opengl_light_state::SetLinearAttenuation(GLfloat val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(LinearAttenuation, val) ) {
		return;
	}

	LinearAttenuation = val;

}

void opengl_light_state::SetQuadraticAttenuation(GLfloat val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(QuadraticAttenuation, val) ) {
		return;
	}

	QuadraticAttenuation = val;

}

void opengl_light_state::SetSpotExponent(GLfloat val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(SpotExponent, val) ) {
		return;
	}

	SpotExponent = val;

}

void opengl_light_state::SetSpotCutoff(GLfloat val)
{
	Assert(Light_num >= 0 && Light_num < GL_max_lights);

	if ( fl_equal(SpotCutoff, val) ) {
		return;
	}

	SpotCutoff = val;

}

void gr_opengl_clear_states()
{
	if ( GL_version >= 30 ) {
		glBindVertexArray(GL_vao);
	}

	gr_zbias(0);
	gr_zbuffer_set(ZBUFFER_TYPE_READ);
	gr_set_cull(0);
	gr_set_fill_mode(GR_FILL_MODE_SOLID);
	gr_reset_lighting();
	gr_set_lighting(false, false);

	opengl_shader_set_current();
}

void opengl_setup_render_states(int &r, int &g, int &b, int &alpha, int &tmap_type, int flags, int is_scaler)
{
	gr_alpha_blend alpha_blend = (gr_alpha_blend)(-1);
	gr_zbuffer_type zbuffer_type = (gr_zbuffer_type)(-1);
	
	if (gr_zbuffering) {
		if ( is_scaler || (gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER) ) {
			zbuffer_type = ZBUFFER_TYPE_READ;
		} else {
			zbuffer_type = ZBUFFER_TYPE_FULL;
		}
	} else {
		zbuffer_type = ZBUFFER_TYPE_NONE;
	}

	tmap_type = TCACHE_TYPE_NORMAL;

	if (flags & TMAP_FLAG_TEXTURED) {
		r = g = b = 255;
	} else {
		r = gr_screen.current_color.red;
		g = gr_screen.current_color.green;
		b = gr_screen.current_color.blue;
	}

	if (flags & TMAP_FLAG_BW_TEXTURE) {
		r = gr_screen.current_color.red;
		g = gr_screen.current_color.green;
		b = gr_screen.current_color.blue;
	}

	if ( gr_screen.current_alphablend_mode == GR_ALPHABLEND_FILTER ) {
		if ( (gr_screen.current_bitmap >= 0) && bm_has_alpha_channel(gr_screen.current_bitmap) ) {
			tmap_type = TCACHE_TYPE_XPARENT;

			alpha_blend = ALPHA_BLEND_ALPHA_BLEND_ALPHA;

			// Blend with screen pixel using src*alpha+dst
			float factor = gr_screen.current_alpha;

			if (factor >= 1.0f) {
				alpha = 255;
			} else {
				alpha = fl2i(gr_screen.current_alpha*255.0f);
			}
		} else {
			tmap_type = TCACHE_TYPE_NORMAL;
			alpha_blend = ALPHA_BLEND_ADDITIVE;	// ALPHA_BLEND_ALPHA_ADDITIVE;

			// Blend with screen pixel using src*alpha+dst
			float factor = gr_screen.current_alpha;

			alpha = 255;

			if (factor < 1.0f) {
				r = fl2i(r * gr_screen.current_alpha);
				g = fl2i(g * gr_screen.current_alpha);
				b = fl2i(b * gr_screen.current_alpha);
			}
		}
	} else {
		alpha_blend = ALPHA_BLEND_ALPHA_BLEND_ALPHA;
		alpha = fl2i(gr_screen.current_alpha * 255.0f);
	}

	
	if (flags & TMAP_FLAG_TEXTURED) {
		// use nonfiltered textures for interface graphics
		if (flags & TMAP_FLAG_INTERFACE) {
			tmap_type = TCACHE_TYPE_INTERFACE;
		}
	}

	GL_state.SetAlphaBlendMode(alpha_blend);
	GL_state.SetZbufferType(zbuffer_type);
}
