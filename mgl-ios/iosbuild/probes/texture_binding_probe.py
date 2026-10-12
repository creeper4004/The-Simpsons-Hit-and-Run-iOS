#!/usr/bin/env python3
"""Exercise the real MGL texture bind/delete functions with a minimal GL state.

No Metal device is needed. This checks state bookkeeping, not rendered pixels.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2] / 'src/mgl/MGL/src/textures.c')
args = parser.parse_args()
source = args.source.read_text()
def function(name):
    start = source.index('void ' + name + '(')
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

harness = r'''
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
typedef unsigned GLuint, GLenum;
typedef int GLint, GLsizei;
#define TEXTURE_UNITS 128
#define _MAX_TEXTURE_TYPES 3
#define DIRTY_TEX_BINDING 1
#define DIRTY_IMAGE_UNIT_STATE 2
#define DIRTY_TEX 4
struct Texture { int released; };
struct ImageUnit { GLuint texture; int marker; };
struct TextureUnit { Texture* textures[_MAX_TEXTURE_TYPES]; };
struct Context { struct {
    GLuint active_texture, active_texture_mask[4], dirty_bits;
    Texture* active_textures[TEXTURE_UNITS];
    TextureUnit texture_units[TEXTURE_UNITS];
    ImageUnit image_units[TEXTURE_UNITS];
} state; };
typedef Context* GLMContext;
#define STATE(field) ctx->state.field
Texture deleted = {}, other = {};
int textureIndexFromTarget(GLMContext, GLenum target) { return target; }
Texture* findTexture(GLMContext, GLuint name) { return name == 1 ? &deleted : name == 2 ? &other : NULL; }
Texture* getTexture(GLMContext ctx, GLenum, GLuint name) { return findTexture(ctx, name); }
void releaseTextureStorage(GLMContext, Texture* tex) { tex->released++; }
'''
harness += function('mglBindTexture') + '\n' + function('mglDeleteTextures')
harness += r'''
int main() {
    Context storage = {}; GLMContext ctx = &storage;
    // Reproduce the crash invariant before testing high-unit bit shifts.
    ctx->state.active_textures[0] = &deleted;
    ctx->state.active_texture_mask[0] = 1;
    ctx->state.texture_units[0].textures[0] = &deleted;
    GLuint first = 1; mglDeleteTextures(ctx, 1, &first);
    assert(ctx->state.active_texture_mask[0] == 0);
    assert(!ctx->state.texture_units[0].textures[0]);
    storage = {}; deleted.released = 0;
    // Exercise every bit of all four mask words, including bits 31/63/95/127.
    for (unsigned i = 0; i < TEXTURE_UNITS; ++i) {
        ctx->state.active_texture = i;
        mglBindTexture(ctx, 0, 1);
        assert(ctx->state.active_textures[i] == &deleted);
        assert(ctx->state.active_texture_mask[i / 32] & (1u << (i % 32)));
        mglBindTexture(ctx, 0, 0);
        assert(!(ctx->state.active_texture_mask[i / 32] & (1u << (i % 32))));
        mglBindTexture(ctx, 0, 1);
        ctx->state.image_units[i] = {1, 99};
    }
    // A deleted texture can also be bound to a non-active target.
    ctx->state.active_texture = 5;
    mglBindTexture(ctx, 1, 2);
    GLuint name = 1;
    mglDeleteTextures(ctx, 1, &name);
    assert(deleted.released == 1);
    for (unsigned i = 0; i < TEXTURE_UNITS; ++i) {
        if (i == 5) {
            assert(ctx->state.active_textures[i] == &other);
            assert(ctx->state.active_texture_mask[0] == (1u << 5));
            assert(ctx->state.texture_units[i].textures[1] == &other);
        } else {
            assert(!ctx->state.active_textures[i]);
            assert(!(ctx->state.active_texture_mask[i / 32] & (1u << (i % 32))));
        }
        assert(!ctx->state.texture_units[i].textures[0]);
        assert(!ctx->state.image_units[i].texture);
        assert(!ctx->state.image_units[i].marker);
    }
    assert(ctx->state.dirty_bits & DIRTY_TEX_BINDING);
    assert(ctx->state.dirty_bits & DIRTY_IMAGE_UNIT_STATE);
    Context before = storage;
    name = 999; mglDeleteTextures(ctx, 1, &name);
    assert(memcmp(&before, &storage, sizeof(storage)) == 0);
    puts("PASS: all 128 mask bits, unbinding, deletion, inactive targets, image units and unrelated bindings");
}
'''
with tempfile.TemporaryDirectory(prefix='mgl-texture-probe-') as directory:
    root = Path(directory)
    (root/'probe.cpp').write_text(harness)
    subprocess.run(['clang++', '-std=c++17', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(root/'probe.cpp'), '-o', str(root/'probe')], check=True)
    subprocess.run([str(root/'probe')], check=True)
