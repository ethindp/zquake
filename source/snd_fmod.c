/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "sound.h"
#include <fmod.h>
#include <fmod_dsp.h>
#include <fmod_errors.h>

#define FMOD_ERRCHECK(result, ctx)                                             \
  do {                                                                         \
    FMOD_RESULT _fr = (result);                                                \
    if (_fr != FMOD_OK) {                                                      \
      Com_Printf("FMOD ERROR [%s]: %s (%d)\n", (ctx), FMOD_ErrorString(_fr),   \
                 (int)_fr);                                                    \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define FMOD_ERRCHECK_R(result, ctx, retval)                                   \
  do {                                                                         \
    FMOD_RESULT _fr = (result);                                                \
    if (_fr != FMOD_OK) {                                                      \
      Com_Printf("FMOD ERROR [%s]: %s (%d)\n", (ctx), FMOD_ErrorString(_fr),   \
                 (int)_fr);                                                    \
      return (retval);                                                         \
    }                                                                          \
  } while (0)

#define FMOD_ERRLOG(result, ctx)                                               \
  do {                                                                         \
    FMOD_RESULT _fr = (result);                                                \
    if (_fr != FMOD_OK) {                                                      \
      Com_Printf("FMOD ERROR [%s]: %s (%d)\n", (ctx), FMOD_ErrorString(_fr),   \
                 (int)_fr);                                                    \
    }                                                                          \
  } while (0)

#define QU_PER_METER 39.37f
#define STATIC_ATTEN_DIV 64.0f
#define MIN_3D_DIST 80.0f
#define MAX_FMOD_CHANNELS 4095
#define NOMINAL_CLIP_DIST 1000.0f
#ifndef ATMOKY_PLUGIN_FILENAME
#define ATMOKY_PLUGIN_FILENAME "atmokyTrueSpatial.dll"
#endif
#define VALID_ENTITY(n) ((n) > 0 && (n) < MAX_EDICTS)
#define MAX_SOUND_VELOCITY_QU 3000.0f
#define TELEPORT_DIST_QU 1000.0f
#define SFX_INITIAL 256
#define SFX_GROW 256

enum atmokySpatializerParameterIndex {
  ATMOKY_PARAMETER_MIN_DISTANCE,       // float
  ATMOKY_PARAMETER_MAX_DISTANCE,       // float
  ATMOKY_PARAMETER_DISTANCE_MODEL,     // FMOD_DSP_PAN_3D_ROLLOFF_TYPE (int)
  ATMOKY_PARAMETER_ATTRIBUTES3D,       // FMOD_DSP_PARAMETER_3DATTRIBUTES*
  ATMOKY_PARAMETER_OUTPUT_FORMAT,      // OutputFormat (int, see below)
  ATMOKY_PARAMETER_GAIN,               // float
  ATMOKY_PARAMETER_LFE_GAIN,           // float
  ATMOKY_PARAMETER_WIDTH,              // float
  ATMOKY_PARAMETER_INNER_ANGLE,        // float
  ATMOKY_PARAMETER_OUTER_ANGLE,        // float
  ATMOKY_PARAMETER_OUTER_GAIN,         // float
  ATMOKY_PARAMETER_OUTER_LOWPASS,      // float
  ATMOKY_PARAMETER_OCCLUSION,          // float
  ATMOKY_PARAMETER_NFE_DISTANCE,       // float
  ATMOKY_PARAMETER_NFE_GAIN,           // float
  ATMOKY_PARAMETER_NFE_BASS_BOOST,     // float
  ATMOKY_PARAMETER_BINAURAL_IF_STEREO, // float
  ATMOKY_PARAMETER_ATTRIBUTES3DMULTI,  // FMOD_DSP_PARAMETER_3DATTRIBUTES_MULTI*
                                       // (added in v2.1.0)
  ATMOKY_PARAMETER_OVERALL_GAIN, // FMOD_DSP_PARAMETER_OVERALLGAIN* (read-only
                                 // parameter, added in v2.1.5)
  ATMOKY_SPATIALIZER_NUM_PARAMETERS,
};

enum AtmokyOutputFormat {
  ATMOKY_OUTPUT_FORMAT_PLATFORM = 0,
  ATMOKY_OUTPUT_FORMAT_STEREO,
  ATMOKY_OUTPUT_FORMAT_BINAURAL,
  ATMOKY_OUTPUT_FORMAT_QUAD,
  ATMOKY_OUTPUT_FORMAT_FIVE_POINT_ZERO,
  ATMOKY_OUTPUT_FORMAT_FIVE_POINT_ONE,
  ATMOKY_OUTPUT_FORMAT_SEVEN_POINT_ONE,
  ATMOKY_OUTPUT_FORMAT_SEVEN_POINT_ONE_POINT_FOUR,
  ATMOKY_OUTPUT_FORMAT_MAX = ATMOKY_OUTPUT_FORMAT_SEVEN_POINT_ONE_POINT_FOUR
};

enum atmokyExternalizerParameterIndex {
  ATMOKY_EXTERNALIZER_AMOUNT,    // float [0..100], default 50
  ATMOKY_EXTERNALIZER_CHARACTER, // float [0..100], default 50
  ATMOKY_EXTERNALIZER_NUM_PARAMETERS,
};

static FMOD_SYSTEM *fmod_system = NULL;
static qbool fmod_initialized = false;
static qbool snd_commands_initialized = false;
static FMOD_OUTPUTTYPE desired_output = FMOD_OUTPUTTYPE_AUTODETECT;
static qbool atmoky_available = false;
static unsigned int atmoky_root_handle = 0;
static unsigned int atmoky_spatializer_handle = 0;
static unsigned int atmoky_externalizer_handle = 0;
static FMOD_DSP *atmoky_master_externalizer = NULL;
static FMOD_3D_ATTRIBUTES listener_atmoky;
static FMOD_VECTOR listener_atmoky_right;

typedef struct {
  FMOD_SOUND *sound;
  qbool loaded; // true = load was attempted (sound may be NULL on failure)
} fmod_sfx_t;

static sfx_t *known_sfx = NULL;
static fmod_sfx_t *fmod_sounds = NULL;
static int max_sfx = 0;
static int num_sfx = 0;
static sfx_t *ambient_sfx[NUM_AMBIENTS];

typedef struct {
  FMOD_CHANNEL *channel;
  FMOD_DSP *spatializer;
  int entnum;
  int entchannel;
  sfx_t *sfx;
  qbool is_static;
  vec3_t origin_qu;
  vec3_t prev_origin_qu;
  vec3_t origin_offset;
  qbool have_prev_origin;
} fmod_channel_t;
static fmod_channel_t fmod_channels[MAX_FMOD_CHANNELS];
static FMOD_CHANNEL *ambient_fmod_channels[NUM_AMBIENTS];
static float ambient_vol[NUM_AMBIENTS];

channel_t channels[MAX_CHANNELS];
int total_channels;
int snd_blocked = 0;
qbool snd_initialized = false;
dma_t dma;
int paintedtime;
int soundtime;
vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;

cvar_t bgmvolume = {"bgmvolume", "1", CVAR_ARCHIVE};
cvar_t s_initsound = {"s_initsound", "1"};
cvar_t s_volume = {"s_volume", "0.7", CVAR_ARCHIVE};
cvar_t s_nosound = {"s_nosound", "0"};
cvar_t s_precache = {"s_precache", "1"};
cvar_t s_loadas8bit = {"s_loadas8bit", "0"};
cvar_t s_khz = {"s_khz", "44", CVAR_ARCHIVE};
cvar_t s_ambientlevel = {"s_ambientlevel", "0.3"};
cvar_t s_ambientfade = {"s_ambientfade", "100"};
cvar_t s_noextraupdate = {"s_noextraupdate", "0"};
cvar_t s_show = {"s_show", "0"};
cvar_t s_mixahead = {"s_mixahead", "0.1", CVAR_ARCHIVE};
cvar_t s_swapstereo = {"s_swapstereo", "0", CVAR_ARCHIVE};
cvar_t s_doppler = {"s_doppler", "1", CVAR_ARCHIVE};
cvar_t s_doppler_factor = {"s_doppler_factor", "1.0", CVAR_ARCHIVE};
cvar_t s_externalizer = {"s_externalizer", "1", CVAR_ARCHIVE};
cvar_t s_externalizer_amount = {"s_externalizer_amount", "50", CVAR_ARCHIVE};
cvar_t s_externalizer_character = {"s_externalizer_character", "50",
                                   CVAR_ARCHIVE};

static void S_Play_f(void);
static void S_PlayVol_f(void);
static void S_StopAllSounds_f(void);
static void S_SoundList_f(void);
static void S_SoundInfo_f(void);
static void S_FMOD_Output_f(void);
static void S_FMOD_Drivers_f(void);
static void S_FMOD_Restart_f(void);
static void S_UpdateAmbientSounds(void);
static FMOD_SOUND *FMOD_LoadSfx(sfx_t *sfx);

typedef struct {
  const char *name;
  FMOD_OUTPUTTYPE type;
} output_entry_t;

static const output_entry_t output_types[] = {
    {"auto", FMOD_OUTPUTTYPE_AUTODETECT},
    {"nosound", FMOD_OUTPUTTYPE_NOSOUND},
#ifdef _WIN32
    {"wasapi", FMOD_OUTPUTTYPE_WASAPI},
    {"asio", FMOD_OUTPUTTYPE_ASIO},
    {"winsonic", FMOD_OUTPUTTYPE_WINSONIC},
#endif
#ifdef __linux__
    {"pulseaudio", FMOD_OUTPUTTYPE_PULSEAUDIO},
    {"alsa", FMOD_OUTPUTTYPE_ALSA},
#endif
#ifdef __APPLE__
    {"coreaudio", FMOD_OUTPUTTYPE_COREAUDIO},
#endif
    {NULL, FMOD_OUTPUTTYPE_AUTODETECT}};

static const char *OutputTypeName(FMOD_OUTPUTTYPE type) {
  int i;
  for (i = 0; output_types[i].name; i++)
    if (output_types[i].type == type)
      return output_types[i].name;
  return "unknown";
}

static qbool GrowSfxArrays(void) {
  if ((uint64_t)(max_sfx + SFX_GROW) >= INT_MAX)
    Sys_Error("Grow SFX: exceeded maximum integer width");
  int32_t new_cap = max_sfx + SFX_GROW;
  sfx_t *new_known;
  fmod_sfx_t *new_fmod;
  new_known = (sfx_t *)Q_malloc(new_cap * sizeof(sfx_t));
  new_fmod = (fmod_sfx_t *)Q_malloc(new_cap * sizeof(fmod_sfx_t));
  if (!new_known || !new_fmod) {
    if (new_known)
      Q_free(new_known);
    if (new_fmod)
      Q_free(new_fmod);
    Com_Printf("GrowSfxArrays: Q_malloc failed for %d slots\n", new_cap);
    return false;
  }
  if (known_sfx && max_sfx > 0) {
    memcpy(new_known, known_sfx, max_sfx * sizeof(sfx_t));
    memcpy(new_fmod, fmod_sounds, max_sfx * sizeof(fmod_sfx_t));
  }
  memset(&new_known[max_sfx], 0, SFX_GROW * sizeof(sfx_t));
  memset(&new_fmod[max_sfx], 0, SFX_GROW * sizeof(fmod_sfx_t));
  for (int i = 0; i < NUM_AMBIENTS; i++) {
    if (ambient_sfx[i]) {
      int idx = (int)(ambient_sfx[i] - known_sfx);
      ambient_sfx[i] = &new_known[idx];
    }
  }
  for (int i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (fmod_channels[i].sfx) {
      int idx = (int)(fmod_channels[i].sfx - known_sfx);
      fmod_channels[i].sfx = &new_known[idx];
    }
  }
  if (known_sfx)
    Q_free(known_sfx);
  if (fmod_sounds)
    Q_free(fmod_sounds);
  known_sfx = new_known;
  fmod_sounds = new_fmod;
  max_sfx = new_cap;
  return true;
}

static FMOD_VECTOR QVec(vec3_t v) {
  FMOD_VECTOR fv;
  fv.x = v[0];
  fv.y = v[1];
  fv.z = v[2];
  return fv;
}

static qbool NormalizeInPlace(FMOD_VECTOR *v) {
  float len = v->x * v->x + v->y * v->y + v->z * v->z;
  if (len < 1e-6f)
    return false;
  len = 1.0f / sqrtf(len);
  v->x *= len;
  v->y *= len;
  v->z *= len;
  return true;
}

static void OrthonormalizeFmodVectors(FMOD_VECTOR *fwd, FMOD_VECTOR *up) {
  float dot = fwd->x * up->x + fwd->y * up->y + fwd->z * up->z;
  up->x -= dot * fwd->x;
  up->y -= dot * fwd->y;
  up->z -= dot * fwd->z;
  NormalizeInPlace(up);
}

static FMOD_VECTOR QToAtmokyPosMeters(const vec3_t qpos) {
  const float qu_to_m = 1.0f / QU_PER_METER;
  FMOD_VECTOR v;
  v.x = -qpos[1] * qu_to_m;
  v.y = qpos[2] * qu_to_m;
  v.z = qpos[0] * qu_to_m;
  return v;
}

static FMOD_VECTOR QToAtmokyVelMeters(const vec3_t qvel) {
  const float qu_to_m = 1.0f / QU_PER_METER;
  FMOD_VECTOR v;
  v.x = -qvel[1] * qu_to_m;
  v.y = qvel[2] * qu_to_m;
  v.z = qvel[0] * qu_to_m;
  return v;
}

static FMOD_VECTOR QToAtmokyDir(const vec3_t qdir) {
  FMOD_VECTOR v;
  v.x = -qdir[1];
  v.y = qdir[2];
  v.z = qdir[0];
  return v;
}

static FMOD_VECTOR FMOD_VecSub(FMOD_VECTOR a, FMOD_VECTOR b) {
  FMOD_VECTOR r;
  r.x = a.x - b.x;
  r.y = a.y - b.y;
  r.z = a.z - b.z;
  return r;
}

static float FMOD_Dot(FMOD_VECTOR a, FMOD_VECTOR b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static FMOD_VECTOR FMOD_Cross(FMOD_VECTOR a, FMOD_VECTOR b) {
  FMOD_VECTOR r;
  r.x = a.y * b.z - a.z * b.y;
  r.y = a.z * b.x - a.x * b.z;
  r.z = a.x * b.y - a.y * b.x;
  return r;
}

static void ComputeRelative3DAttributes(const FMOD_VECTOR *abs_pos,
                                        const FMOD_VECTOR *abs_vel,
                                        FMOD_3D_ATTRIBUTES *out_relative) {
  FMOD_VECTOR delta, vdelta;
  delta = FMOD_VecSub(*abs_pos, listener_atmoky.position);
  out_relative->position.x = FMOD_Dot(delta, listener_atmoky_right);
  out_relative->position.y = FMOD_Dot(delta, listener_atmoky.up);
  out_relative->position.z = FMOD_Dot(delta, listener_atmoky.forward);
  vdelta = FMOD_VecSub(*abs_vel, listener_atmoky.velocity);
  out_relative->velocity.x = FMOD_Dot(vdelta, listener_atmoky_right);
  out_relative->velocity.y = FMOD_Dot(vdelta, listener_atmoky.up);
  out_relative->velocity.z = FMOD_Dot(vdelta, listener_atmoky.forward);
  out_relative->forward.x = 0.0f;
  out_relative->forward.y = 0.0f;
  out_relative->forward.z = 1.0f;
  out_relative->up.x = 0.0f;
  out_relative->up.y = 1.0f;
  out_relative->up.z = 0.0f;
}

static void Atmoky_LoadPlugin(void) {
  int nested = 0;
  FMOD_RESULT result;
  atmoky_available = false;
  atmoky_root_handle = 0;
  atmoky_spatializer_handle = 0;
  atmoky_externalizer_handle = 0;
  if (!fmod_system)
    return;
  result = FMOD_System_LoadPlugin(fmod_system, ATMOKY_PLUGIN_FILENAME,
                                  &atmoky_root_handle, 0);
  if (result != FMOD_OK) {
    Com_Printf("Atmoky: FMOD_System_LoadPlugin failed (%s)\n",
               FMOD_ErrorString(result));
    return;
  }
  result =
      FMOD_System_GetNumNestedPlugins(fmod_system, atmoky_root_handle, &nested);
  if (result != FMOD_OK) {
    Com_Printf("Atmoky: GetNumNestedPlugins failed (%s)\n",
               FMOD_ErrorString(result));
    return;
  }
  for (int i = 0; i < nested; i++) {
    unsigned int h = 0;
    char name[256] = {0};
    FMOD_PLUGINTYPE type;
    if (FMOD_System_GetNestedPlugin(fmod_system, atmoky_root_handle, i, &h) !=
        FMOD_OK)
      continue;
    if (FMOD_System_GetPluginInfo(fmod_system, h, &type, name, sizeof(name),
                                  NULL) != FMOD_OK)
      continue;
    if (type == FMOD_PLUGINTYPE_DSP) {
      if (!strcmp(name, "atmoky Spatializer"))
        atmoky_spatializer_handle = h;
      else if (!strcmp(name, "atmoky Externalizer"))
        atmoky_externalizer_handle = h;
    }
  }
  if (atmoky_spatializer_handle) {
    atmoky_available = true;
    Com_Printf("Atmoky: trueSpatial loaded (Spatializer OK)\n");
  } else {
    Com_Printf("Atmoky: plugin loaded but Spatializer not found\n");
  }
}

static void Atmoky_ReleaseSpatializer(fmod_channel_t *fch) {
  if (!fch || !fch->spatializer)
    return;
  if (fch->channel) {
    FMOD_Channel_RemoveDSP(fch->channel, fch->spatializer);
  }
  FMOD_DSP_Release(fch->spatializer);
  fch->spatializer = NULL;
}

static void Atmoky_AttachSpatializer(fmod_channel_t *fch, FMOD_CHANNEL *channel,
                                     const vec3_t origin_qu, float min_dist_qu,
                                     float max_dist_qu) {
  FMOD_RESULT result;
  FMOD_DSP *dsp = NULL;
  if (!atmoky_available || !atmoky_spatializer_handle || !fch || !channel)
    return;
  FMOD_MODE cmode = 0;
  result = FMOD_Channel_GetMode(channel, &cmode);
  FMOD_ERRLOG(result, "Atmoky GetMode");
  cmode &= ~FMOD_3D;
  cmode |= FMOD_2D;
  result = FMOD_Channel_SetMode(channel, cmode);
  FMOD_ERRLOG(result, "Atmoky SetMode preserve");
  result = FMOD_System_CreateDSPByPlugin(fmod_system, atmoky_spatializer_handle,
                                         &dsp);
  if (result != FMOD_OK || !dsp) {
    Com_Printf("Atmoky: CreateDSPByPlugin failed: %s\n",
               FMOD_ErrorString(result));
    return;
  }
  result = FMOD_Channel_AddDSP(channel, FMOD_CHANNELCONTROL_DSP_TAIL, dsp);
  if (result != FMOD_OK) {
    Com_Printf("Atmoky: Channel_AddDSP failed: %s\n", FMOD_ErrorString(result));
    FMOD_DSP_Release(dsp);
    return;
  }
  fch->spatializer = dsp;
  result = FMOD_DSP_SetParameterInt(dsp, ATMOKY_PARAMETER_OUTPUT_FORMAT,
                                    ATMOKY_OUTPUT_FORMAT_BINAURAL);
  FMOD_ERRLOG(result, "Atmoky set output format");
  const float qu_to_m = 1.0f / QU_PER_METER;
  result = FMOD_DSP_SetParameterFloat(dsp, ATMOKY_PARAMETER_MIN_DISTANCE,
                                      min_dist_qu * qu_to_m);
  FMOD_ERRLOG(result, "Atmoky set min distance");
  result = FMOD_DSP_SetParameterFloat(dsp, ATMOKY_PARAMETER_MAX_DISTANCE,
                                      max_dist_qu * qu_to_m);
  FMOD_ERRLOG(result, "Atmoky set max distance");
  result = FMOD_DSP_SetParameterInt(dsp, ATMOKY_PARAMETER_DISTANCE_MODEL,
                                    FMOD_DSP_PAN_3D_ROLLOFF_LINEAR);
  FMOD_ERRLOG(result, "Atmoky set distance model");
  {
    FMOD_DSP_PARAMETER_3DATTRIBUTES a;
    memset(&a, 0, sizeof(a));
    a.absolute.position = QToAtmokyPosMeters(origin_qu);
    a.absolute.velocity.x = 0.0f;
    a.absolute.velocity.y = 0.0f;
    a.absolute.velocity.z = 0.0f;
    a.absolute.forward.x = 0.0f;
    a.absolute.forward.y = 0.0f;
    a.absolute.forward.z = 1.0f;
    a.absolute.up.x = 0.0f;
    a.absolute.up.y = 1.0f;
    a.absolute.up.z = 0.0f;
    ComputeRelative3DAttributes(&a.absolute.position, &a.absolute.velocity,
                                &a.relative);
    result = FMOD_DSP_SetParameterData(dsp, ATMOKY_PARAMETER_ATTRIBUTES3D, &a,
                                       sizeof(a));
    FMOD_ERRLOG(result, "Atmoky set 3D attributes");
  }
}

static void Atmoky_AttachExternalizer(void) {
  FMOD_RESULT result;
  FMOD_CHANNELGROUP *master = NULL;
  if (!atmoky_available || !atmoky_externalizer_handle || !fmod_system)
    return;
  if (!s_externalizer.value)
    return;
  if (atmoky_master_externalizer)
    return;
  result = FMOD_System_CreateDSPByPlugin(
      fmod_system, atmoky_externalizer_handle, &atmoky_master_externalizer);
  if (result != FMOD_OK || !atmoky_master_externalizer) {
    Com_Printf("Atmoky: CreateDSPByPlugin (Externalizer) failed: %s\n",
               FMOD_ErrorString(result));
    atmoky_master_externalizer = NULL;
    return;
  }
  result = FMOD_System_GetMasterChannelGroup(fmod_system, &master);
  if (result != FMOD_OK || !master) {
    Com_Printf("Atmoky: GetMasterChannelGroup failed: %s\n",
               FMOD_ErrorString(result));
    FMOD_DSP_Release(atmoky_master_externalizer);
    atmoky_master_externalizer = NULL;
    return;
  }
  result = FMOD_ChannelGroup_AddDSP(master, FMOD_CHANNELCONTROL_DSP_TAIL,
                                    atmoky_master_externalizer);
  if (result != FMOD_OK) {
    Com_Printf("Atmoky: ChannelGroup_AddDSP (Externalizer) failed: %s\n",
               FMOD_ErrorString(result));
    FMOD_DSP_Release(atmoky_master_externalizer);
    atmoky_master_externalizer = NULL;
    return;
  }
  result = FMOD_DSP_SetParameterFloat(atmoky_master_externalizer,
                                      ATMOKY_EXTERNALIZER_AMOUNT,
                                      s_externalizer_amount.value);
  FMOD_ERRLOG(result, "Externalizer set amount");
  result = FMOD_DSP_SetParameterFloat(atmoky_master_externalizer,
                                      ATMOKY_EXTERNALIZER_CHARACTER,
                                      s_externalizer_character.value);
  FMOD_ERRLOG(result, "Externalizer set character");
  Com_Printf("Atmoky: Externalizer attached to master bus (amount=%.0f, "
             "character=%.0f)\n",
             s_externalizer_amount.value, s_externalizer_character.value);
}

static void Atmoky_DetachExternalizer(void) {
  FMOD_CHANNELGROUP *master = NULL;
  if (!atmoky_master_externalizer)
    return;
  if (fmod_system) {
    if (FMOD_System_GetMasterChannelGroup(fmod_system, &master) == FMOD_OK &&
        master) {
      FMOD_ChannelGroup_RemoveDSP(master, atmoky_master_externalizer);
    }
  }
  FMOD_DSP_Release(atmoky_master_externalizer);
  atmoky_master_externalizer = NULL;
  Com_Printf("Atmoky: Externalizer detached\n");
}

static void Atmoky_UpdateExternalizer(void) {
  FMOD_RESULT result;
  if (!s_externalizer.value && atmoky_master_externalizer) {
    Atmoky_DetachExternalizer();
    return;
  }
  if (s_externalizer.value && !atmoky_master_externalizer) {
    Atmoky_AttachExternalizer();
    if (!atmoky_master_externalizer)
      return;
  }
  if (!atmoky_master_externalizer)
    return;
  result = FMOD_DSP_SetParameterFloat(atmoky_master_externalizer,
                                      ATMOKY_EXTERNALIZER_AMOUNT,
                                      s_externalizer_amount.value);
  FMOD_ERRLOG(result, "Externalizer update amount");
  result = FMOD_DSP_SetParameterFloat(atmoky_master_externalizer,
                                      ATMOKY_EXTERNALIZER_CHARACTER,
                                      s_externalizer_character.value);
  FMOD_ERRLOG(result, "Externalizer update character");
}

static qbool ChannelIsPlaying(FMOD_CHANNEL *ch) {
  FMOD_BOOL playing = 0;
  if (!ch)
    return false;
  if (FMOD_Channel_IsPlaying(ch, &playing) != FMOD_OK)
    return false;
  return playing ? true : false;
}

static int SfxIndex(sfx_t *sfx) {
  int idx = (int)(sfx - known_sfx);
  if (idx < 0 || idx >= max_sfx)
    return -1;
  return idx;
}

static void Atmoky_UpdateSpatializers(void) {
  int i;
  FMOD_RESULT result;
  if (!atmoky_available)
    return;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    fmod_channel_t *fch;
    vec3_t cur_origin;
    vec3_t cur_vel;
    fch = &fmod_channels[i];
    if (!fch->channel || !fch->spatializer)
      continue;
    if (!ChannelIsPlaying(fch->channel)) {
      Atmoky_ReleaseSpatializer(fch);
      fch->channel = NULL;
      continue;
    }
    VectorCopy(fch->origin_qu, cur_origin);
    VectorClear(cur_vel);
    if (!fch->is_static && VALID_ENTITY(fch->entnum)) {
      vec3_t ent_origin;
      VectorCopy(cl_entities[fch->entnum].lerp_origin, ent_origin);
      VectorAdd(ent_origin, fch->origin_offset, cur_origin);
    }
    if (fch->have_prev_origin && cls.frametime > 0.0f) {
      vec3_t move;
      float dist_sq, speed_sq;
      VectorSubtract(cur_origin, fch->prev_origin_qu, move);
      dist_sq = DotProduct(move, move);
      if (dist_sq > TELEPORT_DIST_QU * TELEPORT_DIST_QU) {
        VectorClear(cur_vel);
        fch->have_prev_origin = false;
      } else {
        VectorScale(move, 1.0f / cls.frametime, cur_vel);
        speed_sq = DotProduct(cur_vel, cur_vel);
        if (speed_sq > MAX_SOUND_VELOCITY_QU * MAX_SOUND_VELOCITY_QU) {
          float scale = MAX_SOUND_VELOCITY_QU / sqrtf(speed_sq);
          VectorScale(cur_vel, scale, cur_vel);
        }
      }
    } else {
      VectorClear(cur_vel);
    }
    VectorCopy(cur_origin, fch->origin_qu);
    VectorCopy(cur_origin, fch->prev_origin_qu);
    fch->have_prev_origin = true;
    FMOD_DSP_PARAMETER_3DATTRIBUTES a;
    memset(&a, 0, sizeof(a));
    a.absolute.position = QToAtmokyPosMeters(cur_origin);
    a.absolute.velocity = QToAtmokyVelMeters(cur_vel);
    a.absolute.forward.x = 0.0f;
    a.absolute.forward.y = 0.0f;
    a.absolute.forward.z = 1.0f;
    a.absolute.up.x = 0.0f;
    a.absolute.up.y = 1.0f;
    a.absolute.up.z = 0.0f;
    ComputeRelative3DAttributes(&a.absolute.position, &a.absolute.velocity,
                                &a.relative);
    result = FMOD_DSP_SetParameterData(
        fch->spatializer, ATMOKY_PARAMETER_ATTRIBUTES3D, &a, sizeof(a));
    FMOD_ERRLOG(result, "Atmoky update 3DAttributes");
  }
}

static void S_UpdateMovingSounds(void) {
  int i;
  FMOD_RESULT result;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    fmod_channel_t *fch = &fmod_channels[i];
    vec3_t cur_origin, vel_qu;
    FMOD_VECTOR pos, vel;
    if (!fch->channel || fch->spatializer)
      continue;
    if (!ChannelIsPlaying(fch->channel)) {
      fch->channel = NULL;
      continue;
    }
    if (fch->is_static)
      continue;
    if (fch->entnum <= 0 || fch->entnum >= MAX_EDICTS)
      continue;
    if (fch->entnum == cl.playernum + 1)
      continue;
    FMOD_MODE ch_mode;
    result = FMOD_Channel_GetMode(fch->channel, &ch_mode);
    FMOD_ERRCHECK(result, "Retrieval of sound mode");
    if (!(ch_mode & FMOD_3D))
      continue;
    vec3_t ent_origin;
    VectorCopy(cl_entities[fch->entnum].lerp_origin, ent_origin);
    VectorAdd(ent_origin, fch->origin_offset, cur_origin);
    if (fch->have_prev_origin && cls.frametime > 0.0f) {
      vec3_t move;
      float dist_sq, speed_sq;
      VectorSubtract(cur_origin, fch->prev_origin_qu, move);
      dist_sq = DotProduct(move, move);
      if (dist_sq > 1000.0f * 1000.0f) {
        VectorClear(vel_qu);
        fch->have_prev_origin = false;
      } else {
        VectorScale(move, 1.0f / cls.frametime, vel_qu);
        speed_sq = DotProduct(vel_qu, vel_qu);
        if (speed_sq > 3000.0f * 3000.0f) {
          float scale = 3000.0f / sqrtf(speed_sq);
          VectorScale(vel_qu, scale, vel_qu);
        }
      }
    } else {
      VectorClear(vel_qu);
    }
    VectorCopy(cur_origin, fch->origin_qu);
    VectorCopy(cur_origin, fch->prev_origin_qu);
    fch->have_prev_origin = true;
    pos = QVec(cur_origin);
    vel = QVec(vel_qu);
    result = FMOD_Channel_Set3DAttributes(fch->channel, &pos, &vel);
    FMOD_ERRCHECK(result, "Update of 3D sounds");
  }
}

static qbool ChannelIsVirtual(FMOD_CHANNEL *ch) {
  FMOD_BOOL virt = 0;
  if (!ch)
    return false;
  if (FMOD_Channel_IsVirtual(ch, &virt) != FMOD_OK)
    return false;
  return virt ? true : false;
}

sfx_t *S_FindName(char *name) {
  int i;
  sfx_t *sfx;

  if (!name)
    Sys_Error("S_FindName: NULL");
  if (strlen(name) >= MAX_QPATH)
    Sys_Error("Sound name too long: %s", name);

  for (i = 0; i < num_sfx; i++)
    if (!strcmp(known_sfx[i].name, name))
      return &known_sfx[i];

  /* need a new slot */
  if (num_sfx >= max_sfx) {
    if (!GrowSfxArrays())
      Sys_Error("S_FindName: couldn't grow sfx arrays");
  }

  sfx = &known_sfx[num_sfx];
  strcpy(sfx->name, name);
  memset(&sfx->cache, 0, sizeof(sfx->cache));
  fmod_sounds[num_sfx].sound = NULL;
  fmod_sounds[num_sfx].loaded = false;
  num_sfx++;
  return sfx;
}

static FMOD_SOUND *FMOD_LoadSfx(sfx_t *sfx) {
  int idx;
  char namebuf[256];
  byte stackbuf[4 * 1024];
  byte *data;
  FMOD_CREATESOUNDEXINFO exinfo;
  FMOD_RESULT result;
  if (!fmod_system || !sfx)
    return NULL;
  idx = SfxIndex(sfx);
  if (idx < 0)
    return NULL;
  if (fmod_sounds[idx].loaded)
    return fmod_sounds[idx].sound;
  snprintf(namebuf, sizeof(namebuf), "sound/%s", sfx->name);
  data = FS_LoadStackFile(namebuf, stackbuf, sizeof(stackbuf));
  if (!data) {
    Com_Printf("FMOD: couldn't load %s\n", namebuf);
    fmod_sounds[idx].loaded = true; // don't retry
    fmod_sounds[idx].sound = NULL;
    return NULL;
  }
  memset(&exinfo, 0, sizeof(exinfo));
  exinfo.cbsize = sizeof(exinfo);
  exinfo.length = fs_filesize;
  FMOD_MODE mode;
  mode = FMOD_OPENMEMORY | FMOD_LOOP_OFF | FMOD_CREATESAMPLE;
  mode |= (atmoky_available ? FMOD_2D : (FMOD_3D | FMOD_3D_LINEARROLLOFF));
  result = FMOD_System_CreateSound(fmod_system, (const char *)data, mode,
                                   &exinfo, &fmod_sounds[idx].sound);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [CreateSound '%s']: %s (%d)\n", sfx->name,
               FMOD_ErrorString(result), (int)result);
    fmod_sounds[idx].sound = NULL;
  } else {
    if (!atmoky_available) {
      result = FMOD_Sound_Set3DMinMaxDistance(fmod_sounds[idx].sound,
                                              MIN_3D_DIST, NOMINAL_CLIP_DIST);
      FMOD_ERRLOG(result, "Set3DMinMaxDistance default");
    }
  }
  fmod_sounds[idx].loaded = true;
  return fmod_sounds[idx].sound;
}

/*
================
S_LoadSound

Engine compatibility wrapper.  Returns non-NULL on success.
The returned pointer must NOT be dereferenced as sfxcache_t;
it is only used as a success/failure indicator by callers.
================
*/
sfxcache_t *S_LoadSound(sfx_t *s) { return (sfxcache_t *)FMOD_LoadSfx(s); }

/*
================
S_PrecacheSound
================
*/
sfx_t *S_PrecacheSound(char *name) {
  sfx_t *sfx;
  if (!fmod_initialized || s_nosound.value)
    return NULL;
  sfx = S_FindName(name);
  if (s_precache.value)
    FMOD_LoadSfx(sfx);
  return sfx;
}

/*
================
S_TouchSound
================
*/
void S_TouchSound(char *name) {
  if (!fmod_initialized)
    return;
  S_FindName(name);
}

/*
================
FindChannel

Search tracked channels for one matching entnum/entchannel.
entchannel == -1 matches any channel from that entity.
Returns NULL if none found.
================
*/
static fmod_channel_t *FindChannel(int entnum, int entchannel) {
  int i;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (!fmod_channels[i].channel)
      continue;
    if (fmod_channels[i].entnum != entnum)
      continue;
    if (entchannel != -1 && fmod_channels[i].entchannel != entchannel)
      continue;
    if (!ChannelIsPlaying(fmod_channels[i].channel)) {
      Atmoky_ReleaseSpatializer(&fmod_channels[i]);
      fmod_channels[i].channel = NULL;
      continue;
    }
    return &fmod_channels[i];
  }
  return NULL;
}

/*
================
AllocChannel

Find a free slot, or steal the oldest non-player channel.
================
*/
static fmod_channel_t *AllocChannel(void) {
  int i;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (!fmod_channels[i].channel)
      return &fmod_channels[i];
    if (!ChannelIsPlaying(fmod_channels[i].channel)) {
      Atmoky_ReleaseSpatializer(&fmod_channels[i]);
      memset(&fmod_channels[i], 0, sizeof(fmod_channels[i]));
      return &fmod_channels[i];
    }
  }
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (!fmod_channels[i].channel)
      continue;
    if (!ChannelIsVirtual(fmod_channels[i].channel))
      continue;
    FMOD_Channel_Stop(fmod_channels[i].channel);
    Atmoky_ReleaseSpatializer(&fmod_channels[i]);
    memset(&fmod_channels[i], 0, sizeof(fmod_channels[i]));
    return &fmod_channels[i];
  }
  return NULL;
}

/*
================
StopAllTrackedChannels
================
*/
static void StopAllTrackedChannels(void) {
  int i;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (fmod_channels[i].channel) {
      FMOD_Channel_Stop(fmod_channels[i].channel);
    }
    Atmoky_ReleaseSpatializer(&fmod_channels[i]);
  }
  memset(fmod_channels, 0, sizeof(fmod_channels));
}

/*
================
StopAmbientChannels
================
*/
static void StopAmbientChannels(void) {
  int i;
  for (i = 0; i < NUM_AMBIENTS; i++) {
    if (ambient_fmod_channels[i]) {
      FMOD_Channel_Stop(ambient_fmod_channels[i]);
      ambient_fmod_channels[i] = NULL;
    }
    ambient_vol[i] = 0;
  }
}

static void S_UpdateAmbientSounds(void) {
  struct cleaf_s *leaf;
  float target;
  int i;
  FMOD_RESULT result;
  FMOD_SOUND *snd;
  if (cls.state != ca_active) {
    StopAmbientChannels();
    return;
  }
  leaf = CM_PointInLeaf(listener_origin);
  if (!CM_Leafnum(leaf) || !s_ambientlevel.value) {
    for (i = 0; i < NUM_AMBIENTS; i++) {
      if (ambient_fmod_channels[i])
        FMOD_Channel_SetVolume(ambient_fmod_channels[i], 0.0f);
      ambient_vol[i] = 0;
    }
    return;
  }
  for (i = 0; i < NUM_AMBIENTS; i++) {
    if (!ambient_sfx[i])
      continue;
    target = s_ambientlevel.value * CM_LeafAmbientLevel(leaf, i);
    if (target < 8.0f)
      target = 0.0f;
    if (ambient_vol[i] < target) {
      ambient_vol[i] += cls.frametime * s_ambientfade.value;
      if (ambient_vol[i] > target)
        ambient_vol[i] = target;
    } else if (ambient_vol[i] > target) {
      ambient_vol[i] -= cls.frametime * s_ambientfade.value;
      if (ambient_vol[i] < target)
        ambient_vol[i] = target;
    }
    if (!ChannelIsPlaying(ambient_fmod_channels[i])) {
      snd = FMOD_LoadSfx(ambient_sfx[i]);
      if (!snd)
        continue;
      result = FMOD_System_PlaySound(fmod_system, snd, NULL, 1,
                                     &ambient_fmod_channels[i]);
      if (result != FMOD_OK) {
        Com_Printf("FMOD ERROR [ambient %d PlaySound]: %s (%d)\n", i,
                   FMOD_ErrorString(result), (int)result);
        ambient_fmod_channels[i] = NULL;
        continue;
      }
      result = FMOD_Channel_SetMode(ambient_fmod_channels[i],
                                    FMOD_2D | FMOD_LOOP_NORMAL);
      FMOD_ERRLOG(result, "ambient SetMode");
      result = FMOD_Channel_SetLoopCount(ambient_fmod_channels[i], -1);
      FMOD_ERRLOG(result, "ambient SetLoopCount");
      result = FMOD_Channel_SetPaused(ambient_fmod_channels[i], 0);
      FMOD_ERRLOG(result, "ambient SetPaused");
    }
    if (ambient_fmod_channels[i]) {
      result = FMOD_Channel_SetVolume(ambient_fmod_channels[i],
                                      ambient_vol[i] / 255.0f);
      FMOD_ERRLOG(result, "ambient SetVolume");
    }
  }
}

/*
================
S_Init
================
*/
void S_Init(void) {
  FMOD_RESULT result;
  if (!snd_commands_initialized) {
    snd_commands_initialized = true;
    Cvar_Register(&bgmvolume);
    Cvar_Register(&s_volume);
    Cvar_Register(&s_initsound);
    Cvar_Register(&s_nosound);
    Cvar_Register(&s_precache);
    Cvar_Register(&s_loadas8bit);
    Cvar_Register(&s_khz);
    Cvar_Register(&s_ambientlevel);
    Cvar_Register(&s_ambientfade);
    Cvar_Register(&s_noextraupdate);
    Cvar_Register(&s_show);
    Cvar_Register(&s_mixahead);
    Cvar_Register(&s_swapstereo);
    Cvar_Register(&s_doppler);
    Cvar_Register(&s_doppler_factor);
    Cvar_Register(&s_externalizer);
    Cvar_Register(&s_externalizer_amount);
    Cvar_Register(&s_externalizer_character);
    Cmd_AddLegacyCommand("volume", "s_volume");
    Cmd_AddLegacyCommand("nosound", "s_nosound");
    Cmd_AddLegacyCommand("precache", "s_precache");
    Cmd_AddLegacyCommand("loadas8bit", "s_loadas8bit");
    Cmd_AddLegacyCommand("ambient_level", "s_ambientlevel");
    Cmd_AddLegacyCommand("ambient_fade", "s_ambientfade");
    Cmd_AddLegacyCommand("snd_noextraupdate", "s_noextraupdate");
    Cmd_AddLegacyCommand("snd_show", "s_show");
    Cmd_AddLegacyCommand("_snd_mixahead", "s_mixahead");
    Cmd_AddCommand("play", S_Play_f);
    Cmd_AddCommand("playvol", S_PlayVol_f);
    Cmd_AddCommand("stopsound", S_StopAllSounds_f);
    Cmd_AddCommand("soundlist", S_SoundList_f);
    Cmd_AddCommand("soundinfo", S_SoundInfo_f);
    Cmd_AddCommand("s_fmod_output", S_FMOD_Output_f);
    Cmd_AddCommand("s_fmod_drivers", S_FMOD_Drivers_f);
    Cmd_AddCommand("s_fmod_restart", S_FMOD_Restart_f);
  }
  if (!s_initsound.value || COM_CheckParm("-nosound") || s_nosound.value) {
    Com_Printf("Sound initialization skipped\n");
    return;
  }
  result = FMOD_System_Create(&fmod_system, FMOD_VERSION);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [System_Create]: %s (%d)\n",
               FMOD_ErrorString(result), (int)result);
    fmod_system = NULL;
    return;
  }
  result = FMOD_System_SetOutput(fmod_system, desired_output);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [SetOutput '%s']: %s (%d)\n",
               OutputTypeName(desired_output), FMOD_ErrorString(result),
               (int)result);
    Com_Printf("Falling back to auto-detect\n");
    result = FMOD_System_SetOutput(fmod_system, FMOD_OUTPUTTYPE_AUTODETECT);
    FMOD_ERRLOG(result, "SetOutput fallback");
    desired_output = FMOD_OUTPUTTYPE_AUTODETECT;
  }
  Atmoky_LoadPlugin(); /* sets atmoky_available true/false */
  result =
      FMOD_System_Init(fmod_system, MAX_FMOD_CHANNELS,
                       atmoky_available ? 0 : FMOD_INIT_3D_RIGHTHANDED, NULL);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [System_Init]: %s (%d)\n", FMOD_ErrorString(result),
               (int)result);
    FMOD_System_Release(fmod_system);
    fmod_system = NULL;
    return;
  }

  result = FMOD_System_Set3DSettings(
      fmod_system, s_doppler.value ? s_doppler_factor.value : 0.0f,
      atmoky_available ? 1.0f : QU_PER_METER, 1.0f);
  FMOD_ERRLOG(result, "Set3DSettings");
  memset(&dma, 0, sizeof(dma));
  dma.channels = 2;
  dma.samplebits = 16;
  dma.speed = 44100;
  if (!known_sfx) {
    known_sfx = (sfx_t *)Q_malloc(SFX_INITIAL * sizeof(sfx_t));
    fmod_sounds = (fmod_sfx_t *)Q_malloc(SFX_INITIAL * sizeof(fmod_sfx_t));
    if (!known_sfx || !fmod_sounds)
      Sys_Error("S_Init: couldn't allocate sfx arrays");
    memset(known_sfx, 0, SFX_INITIAL * sizeof(sfx_t));
    memset(fmod_sounds, 0, SFX_INITIAL * sizeof(fmod_sfx_t));
    max_sfx = SFX_INITIAL;
    num_sfx = 0;
  }
  fmod_initialized = true;
  snd_initialized = true;
  {
    FMOD_OUTPUTTYPE actual;
    int ndrivers;
    char drvname[256];
    int drvrate;
    FMOD_System_GetOutput(fmod_system, &actual);
    FMOD_System_GetNumDrivers(fmod_system, &ndrivers);
    drvname[0] = '\0';
    if (ndrivers > 0)
      FMOD_System_GetDriverInfo(fmod_system, 0, drvname, sizeof(drvname), NULL,
                                &drvrate, NULL, NULL);
    Com_Printf("FMOD sound system initialized\n");
    Com_Printf("  Output : %s\n", OutputTypeName(actual));
    Com_Printf("  Driver : %s\n", drvname[0] ? drvname : "(none)");
    Com_Printf("  Doppler: %s (factor %.2f)\n", s_doppler.value ? "on" : "off",
               s_doppler_factor.value);
  }
  Atmoky_AttachExternalizer();
  ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
  ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");
  S_StopAllSounds(true);
}

/*
================
S_Startup
================
*/
void S_Startup(void) {}

/*
================
S_Shutdown
================
*/
void S_Shutdown(void) {
  int i;
  if (!fmod_initialized)
    return;
  StopAllTrackedChannels();
  StopAmbientChannels();
  for (i = 0; i < num_sfx; i++) {
    if (fmod_sounds[i].sound) {
      FMOD_Sound_Release(fmod_sounds[i].sound);
      fmod_sounds[i].sound = NULL;
    }
    fmod_sounds[i].loaded = false;
  }
  if (atmoky_master_externalizer) {
    Atmoky_DetachExternalizer();
  }
  if (atmoky_root_handle) {
    FMOD_System_UnloadPlugin(fmod_system, atmoky_root_handle);
    atmoky_root_handle = 0;
  }
  atmoky_available = false;
  atmoky_spatializer_handle = 0;
  atmoky_externalizer_handle = 0;
  if (fmod_system) {
    FMOD_System_Close(fmod_system);
    FMOD_System_Release(fmod_system);
    fmod_system = NULL;
  }
  if (known_sfx) {
    Q_free(known_sfx);
    known_sfx = NULL;
  }
  if (fmod_sounds) {
    Q_free(fmod_sounds);
    fmod_sounds = NULL;
  }
  max_sfx = 0;
  num_sfx = 0;
  fmod_initialized = false;
  snd_initialized = false;
  Com_Printf("FMOD sound system shut down\n");
}

/*
================
S_Restart
================
*/
void S_Restart(void) {
  S_Shutdown();
  S_Init();
}

/*
================
S_StartSound
================
*/
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin,
                  float fvol, float attenuation) {
  FMOD_SOUND *sound;
  FMOD_CHANNEL *channel;
  FMOD_VECTOR pos, vel;
  FMOD_RESULT result;
  fmod_channel_t *fch;
  channel = NULL;
  vel.x = vel.y = vel.z = 0.0f;
  if (!fmod_initialized || !sfx || s_nosound.value)
    return;
  sound = FMOD_LoadSfx(sfx);
  if (!sound)
    return;
  if (entchannel != 0) {
    for (;;) {
      fch = FindChannel(entnum, entchannel);
      if (!fch)
        break;
      FMOD_Channel_Stop(fch->channel);
      Atmoky_ReleaseSpatializer(fch);
      fch->channel = NULL;
    }
  }
  result = FMOD_System_PlaySound(fmod_system, sound, NULL, 1, &channel);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [StartSound PlaySound '%s']: %s (%d)\n", sfx->name,
               FMOD_ErrorString(result), (int)result);
    return;
  }
  fch = AllocChannel();
  if (fch) {
    memset(fch, 0, sizeof(*fch));
    fch->channel = channel;
    fch->entnum = entnum;
    fch->entchannel = entchannel;
    fch->sfx = sfx;
    fch->is_static = false;
    VectorCopy(origin, fch->origin_qu);
    VectorCopy(origin, fch->prev_origin_qu);
    fch->have_prev_origin = false;
    if (VALID_ENTITY(entnum) && entnum != (cl.playernum + 1)) {
      VectorSubtract(origin, cl_entities[entnum].lerp_origin,
                     fch->origin_offset);
    } else {
      VectorClear(fch->origin_offset);
    }
  }
  if (entnum == cl.playernum + 1 || attenuation <= 0) {
    result = FMOD_Channel_SetMode(channel, FMOD_2D);
    FMOD_ERRLOG(result, "StartSound SetMode 2D");
  } else if (atmoky_available && fch) {
    float min_dist, max_dist;
    min_dist = MIN_3D_DIST;
    max_dist = NOMINAL_CLIP_DIST / attenuation;
    if (max_dist < min_dist)
      max_dist = min_dist + 1.0f;
    Atmoky_AttachSpatializer(fch, channel, origin, min_dist, max_dist);
  } else {
    float min_dist, max_dist;
    min_dist = MIN_3D_DIST;
    max_dist = NOMINAL_CLIP_DIST / attenuation;
    if (max_dist < min_dist)
      max_dist = min_dist + 1.0f;
    pos = QVec(origin);
    result = FMOD_Channel_Set3DAttributes(channel, &pos, &vel);
    FMOD_ERRLOG(result, "StartSound Set3DAttributes");
    result = FMOD_Channel_Set3DMinMaxDistance(channel, min_dist, max_dist);
    FMOD_ERRLOG(result, "StartSound Set3DMinMaxDistance");
  }
  result = FMOD_Channel_SetVolume(channel, fvol);
  FMOD_ERRLOG(result, "StartSound SetVolume");
  result = FMOD_Channel_SetPaused(channel, 0);
  FMOD_ERRLOG(result, "StartSound SetPaused");
}

/*
================
S_StaticSound
================
*/
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation) {
  FMOD_SOUND *sound;
  FMOD_CHANNEL *channel = NULL;
  FMOD_VECTOR pos, vel = {0, 0, 0};
  FMOD_RESULT result;
  fmod_channel_t *fch;
  float min_dist, max_dist;
  if (!fmod_initialized || !sfx)
    return;
  sound = FMOD_LoadSfx(sfx);
  if (!sound)
    return;
  result = FMOD_System_PlaySound(fmod_system, sound, NULL, 1, &channel);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [StaticSound PlaySound '%s']: %s (%d)\n", sfx->name,
               FMOD_ErrorString(result), (int)result);
    return;
  }
  if (atmoky_available) {
    result = FMOD_Channel_SetMode(channel, FMOD_2D | FMOD_LOOP_NORMAL);
    FMOD_ERRLOG(result, "StaticSound SetMode 2D loop");
    result = FMOD_Channel_SetLoopCount(channel, -1);
    FMOD_ERRLOG(result, "StaticSound SetLoopCount");
    min_dist = MIN_3D_DIST;
    if (attenuation > 0) {
      max_dist = (NOMINAL_CLIP_DIST * STATIC_ATTEN_DIV) / attenuation;
      if (max_dist < min_dist)
        max_dist = min_dist + 1.0f;
    } else {
      max_dist = 100000.0f; // essentially infinite
    }
    fch = AllocChannel();
    if (fch) {
      memset(fch, 0, sizeof(*fch));
      fch->channel = channel;
      fch->entnum = 0;
      fch->entchannel = 0;
      fch->sfx = sfx;
      fch->is_static = true;
      VectorCopy(origin, fch->origin_qu);
      VectorCopy(origin, fch->prev_origin_qu);
      fch->have_prev_origin = true;
      Atmoky_AttachSpatializer(fch, channel, origin, min_dist, max_dist);
    }
    result = FMOD_Channel_SetVolume(channel, vol / 255.0f);
    FMOD_ERRLOG(result, "StaticSound SetVolume");
    result = FMOD_Channel_SetPaused(channel, 0);
    FMOD_ERRLOG(result, "StaticSound SetPaused");
  } else {
    result = FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARROLLOFF |
                                               FMOD_LOOP_NORMAL);
    FMOD_ERRLOG(result, "StaticSound SetMode");
    result = FMOD_Channel_SetLoopCount(channel, -1);
    FMOD_ERRLOG(result, "StaticSound SetLoopCount");
    pos = QVec(origin);
    result = FMOD_Channel_Set3DAttributes(channel, &pos, &vel);
    FMOD_ERRLOG(result, "StaticSound Set3DAttributes");
    min_dist = MIN_3D_DIST;
    if (attenuation > 0) {
      max_dist = (NOMINAL_CLIP_DIST * STATIC_ATTEN_DIV) / attenuation;
      if (max_dist < min_dist)
        max_dist = min_dist + 1.0f;
    } else {
      max_dist = 100000.0f; // essentially infinite
    }
    result = FMOD_Channel_Set3DMinMaxDistance(channel, min_dist, max_dist);
    FMOD_ERRLOG(result, "StaticSound Set3DMinMaxDistance");
    result = FMOD_Channel_SetVolume(channel, vol / 255.0f);
    FMOD_ERRLOG(result, "StaticSound SetVolume");
    result = FMOD_Channel_SetPaused(channel, 0);
    FMOD_ERRLOG(result, "StaticSound SetPaused");
    fch = AllocChannel();
    if (fch) {
      memset(fch, 0, sizeof(*fch));
      fch->channel = channel;
      fch->entnum = 0;
      fch->entchannel = 0;
      fch->sfx = sfx;
      fch->is_static = true;
    }
  }
}

/*
================
S_StopSound
================
*/
void S_StopSound(int entnum, int entchannel) {
  int i;
  for (i = 0; i < MAX_FMOD_CHANNELS; i++) {
    if (!fmod_channels[i].channel)
      continue;
    if (fmod_channels[i].entnum != entnum)
      continue;
    if (entchannel != -1 && fmod_channels[i].entchannel != entchannel)
      continue;
    FMOD_Channel_Stop(fmod_channels[i].channel);
    Atmoky_ReleaseSpatializer(&fmod_channels[i]);
    fmod_channels[i].channel = NULL;
  }
}

/*
================
S_StopAllSounds
================
*/
void S_StopAllSounds(qbool clear) {
  if (!fmod_initialized)
    return;
  StopAllTrackedChannels();
  StopAmbientChannels();
  total_channels = 0;
}

/*
================
S_Update
================
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up) {
  FMOD_VECTOR fpos, fvel, ffwd, fup;
  FMOD_RESULT result;
  FMOD_CHANNELGROUP *master;
  if (!fmod_initialized || snd_blocked > 0)
    return;
  VectorCopy(origin, listener_origin);
  VectorCopy(forward, listener_forward);
  VectorCopy(right, listener_right);
  VectorCopy(up, listener_up);
  if (atmoky_available) {
    listener_atmoky.position = QToAtmokyPosMeters(origin);
    if (cls.state == ca_active)
      listener_atmoky.velocity = QToAtmokyVelMeters(cl.simvel);
    else
      memset(&listener_atmoky.velocity, 0, sizeof(FMOD_VECTOR));
    listener_atmoky.forward = QToAtmokyDir(forward);
    listener_atmoky.up = QToAtmokyDir(up);
    if (!NormalizeInPlace(&listener_atmoky.forward) ||
        !NormalizeInPlace(&listener_atmoky.up)) {
      listener_atmoky.forward.x = 0.0f;
      listener_atmoky.forward.y = 0.0f;
      listener_atmoky.forward.z = 1.0f;
      listener_atmoky.up.x = 0.0f;
      listener_atmoky.up.y = 1.0f;
      listener_atmoky.up.z = 0.0f;
    } else {
      OrthonormalizeFmodVectors(&listener_atmoky.forward, &listener_atmoky.up);
    }
    listener_atmoky_right =
        FMOD_Cross(listener_atmoky.up, listener_atmoky.forward);
  }
  fpos = QVec(origin);
  ffwd = QVec(forward);
  fup = QVec(up);
  if (cls.state == ca_active)
    fvel = QVec(cl.simvel);
  else
    memset(&fvel, 0, sizeof(fvel));
  if (!NormalizeInPlace(&ffwd) || !NormalizeInPlace(&fup)) {
    ffwd.x = 0.0f;
    ffwd.y = 0.0f;
    ffwd.z = 1.0f; /* +Z forward in FMOD space */
    fup.x = 0.0f;
    fup.y = 1.0f;
    fup.z = 0.0f; /* +Y up in FMOD space      */
  } else {
    OrthonormalizeFmodVectors(&ffwd, &fup);
  }
  result = FMOD_System_Set3DListenerAttributes(fmod_system, 0, &fpos, &fvel,
                                               &ffwd, &fup);
  FMOD_ERRLOG(result, "Set3DListenerAttributes");
  result = FMOD_System_Set3DSettings(
      fmod_system, s_doppler.value ? s_doppler_factor.value : 0.0f,
      QU_PER_METER, 1.0f);
  FMOD_ERRLOG(result, "Set3DSettings");
  result = FMOD_System_GetMasterChannelGroup(fmod_system, &master);
  if (result == FMOD_OK && master) {
    result = FMOD_ChannelGroup_SetVolume(master, s_volume.value);
    FMOD_ERRLOG(result, "master SetVolume");
  }
  S_UpdateAmbientSounds();
  S_UpdateMovingSounds(); /* <--- ADD THIS */
  if (atmoky_available) {
    Atmoky_UpdateSpatializers();
    Atmoky_UpdateExternalizer();
  }
  if (s_show.value) {
    int nplaying = 0;
    FMOD_System_GetChannelsPlaying(fmod_system, &nplaying, NULL);
    Com_Printf("----(%d channels)----\n", nplaying);
  }
  result = FMOD_System_Update(fmod_system);
  FMOD_ERRLOG(result, "System_Update");
}

/*
================
S_ExtraUpdate
================
*/
void S_ExtraUpdate(void) {
  FMOD_RESULT result;
  if (!fmod_initialized || s_noextraupdate.value || snd_blocked > 0)
    return;
  result = FMOD_System_Update(fmod_system);
  FMOD_ERRLOG(result, "ExtraUpdate System_Update");
}

/*
================
S_LocalSound

Play a UI / menu sound (2D, no spatialization)
================
*/
void S_LocalSound(char *sound) {
  sfx_t *sfx;
  FMOD_SOUND *snd;
  FMOD_CHANNEL *channel = NULL;
  FMOD_RESULT result;
  if (!fmod_initialized || s_nosound.value)
    return;
  sfx = S_PrecacheSound(sound);
  if (!sfx) {
    Com_Printf("S_LocalSound: can't cache %s\n", sound);
    return;
  }
  snd = FMOD_LoadSfx(sfx);
  if (!snd)
    return;
  result = FMOD_System_PlaySound(fmod_system, snd, NULL, 1, &channel);
  if (result != FMOD_OK) {
    Com_Printf("FMOD ERROR [LocalSound PlaySound '%s']: %s (%d)\n", sound,
               FMOD_ErrorString(result), (int)result);
    return;
  }
  result = FMOD_Channel_SetMode(channel, FMOD_2D);
  FMOD_ERRLOG(result, "LocalSound SetMode");
  result = FMOD_Channel_SetVolume(channel, 1.0f);
  FMOD_ERRLOG(result, "LocalSound SetVolume");
  result = FMOD_Channel_SetPaused(channel, 0);
  FMOD_ERRLOG(result, "LocalSound SetPaused");
}

/*
================
S_ClearBuffer
================
*/
void S_ClearBuffer(void) {}

/*
================
S_BlockSound
================
*/
void S_BlockSound(void) {
  FMOD_CHANNELGROUP *master;
  FMOD_RESULT result;
  snd_blocked++;
  if (snd_blocked == 1 && fmod_system) {
    result = FMOD_System_GetMasterChannelGroup(fmod_system, &master);
    if (result == FMOD_OK && master) {
      result = FMOD_ChannelGroup_SetMute(master, 1);
      FMOD_ERRLOG(result, "BlockSound SetMute");
    }
  }
}

/*
================
S_UnblockSound
================
*/
void S_UnblockSound(void) {
  FMOD_CHANNELGROUP *master;
  FMOD_RESULT result;
  if (snd_blocked <= 0)
    return;
  snd_blocked--;
  if (snd_blocked == 0 && fmod_system) {
    result = FMOD_System_GetMasterChannelGroup(fmod_system, &master);
    if (result == FMOD_OK && master) {
      result = FMOD_ChannelGroup_SetMute(master, 0);
      FMOD_ERRLOG(result, "UnblockSound SetMute");
    }
  }
}

/*
================
S_Play_f
================
*/
static void S_Play_f(void) {
  static int hash = 345;
  int i;
  char name[256];
  sfx_t *sfx;
  if (!fmod_initialized || s_nosound.value)
    return;
  for (i = 1; i < Cmd_Argc(); i++) {
    strcpy(name, Cmd_Argv(i));
    COM_DefaultExtension(name, ".wav");
    sfx = S_FindName(name);
    S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 0.0);
  }
}

/*
================
S_PlayVol_f
================
*/
static void S_PlayVol_f(void) {
  static int hash = 543;
  int i;
  float vol;
  char name[256];
  sfx_t *sfx;
  if (!fmod_initialized || s_nosound.value)
    return;
  for (i = 1; i < Cmd_Argc(); i += 2) {
    strcpy(name, Cmd_Argv(i));
    COM_DefaultExtension(name, ".wav");
    sfx = S_FindName(name);
    vol = (i + 1 < Cmd_Argc()) ? Q_atof(Cmd_Argv(i + 1)) : 1.0f;
    S_StartSound(hash++, 0, sfx, listener_origin, vol, 0.0);
  }
}

/*
================
S_StopAllSounds_f
================
*/
static void S_StopAllSounds_f(void) { S_StopAllSounds(true); }

/*
================
S_SoundList_f
================
*/
static void S_SoundList_f(void) {
  int i;
  int loaded = 0;
  unsigned int len_ms;
  FMOD_RESULT result;
  Com_Printf("--- Loaded Sounds ---\n");
  for (i = 0; i < num_sfx; i++) {
    if (!fmod_sounds[i].loaded) {
      Com_Printf("  [ ] %s\n", known_sfx[i].name);
      continue;
    }
    if (!fmod_sounds[i].sound) {
      Com_Printf("  [!] %s (load failed)\n", known_sfx[i].name);
      continue;
    }
    len_ms = 0;
    result =
        FMOD_Sound_GetLength(fmod_sounds[i].sound, &len_ms, FMOD_TIMEUNIT_MS);
    if (result != FMOD_OK)
      len_ms = 0;
    Com_Printf("  [*] %s (%.1fs)\n", known_sfx[i].name,
               (float)len_ms / 1000.0f);
    loaded++;
  }
  Com_Printf("Total: %d sounds (%d loaded)\n", num_sfx, loaded);
}

/*
================
S_SoundInfo_f
================
*/
static void S_SoundInfo_f(void) {
  FMOD_OUTPUTTYPE output;
  int ndrivers, nplaying, swchannels;
  char drvname[256];
  int drvrate, drvchan;
  FMOD_SPEAKERMODE drvmode;
  unsigned int version, buildnumber;
  FMOD_RESULT result;
  if (!fmod_initialized) {
    Com_Printf("FMOD sound system not initialized\n");
    return;
  }
  Com_Printf("FMOD Sound Info:\n");
  result = FMOD_System_GetVersion(fmod_system, &version, &buildnumber);
  if (result == FMOD_OK)
    Com_Printf("  Version      : %08x, build %d\n", version, buildnumber);
  result = FMOD_System_GetOutput(fmod_system, &output);
  if (result == FMOD_OK)
    Com_Printf("  Output       : %s\n", OutputTypeName(output));
  result = FMOD_System_GetNumDrivers(fmod_system, &ndrivers);
  if (result == FMOD_OK && ndrivers > 0) {
    result = FMOD_System_GetDriverInfo(fmod_system, 0, drvname, sizeof(drvname),
                                       NULL, &drvrate, &drvmode, &drvchan);
    if (result == FMOD_OK) {
      Com_Printf("  Driver       : %s\n", drvname);
      Com_Printf("  Sample rate  : %d Hz\n", drvrate);
      Com_Printf("  Speaker ch   : %d\n", drvchan);
    }
  }
  result = FMOD_System_GetSoftwareChannels(fmod_system, &swchannels);
  if (result == FMOD_OK)
    Com_Printf("  SW channels  : %d\n", swchannels);
  result = FMOD_System_GetChannelsPlaying(fmod_system, &nplaying, NULL);
  if (result == FMOD_OK)
    Com_Printf("  Playing      : %d\n", nplaying);
  Com_Printf("  Volume       : %.2f\n", s_volume.value);
  Com_Printf("  Doppler      : %s (factor %.2f)\n",
             s_doppler.value ? "on" : "off", s_doppler_factor.value);
  Com_Printf(" Sounds loaded: %d / %d\n", num_sfx, max_sfx);
  if (atmoky_available) {
    Com_Printf("  Atmoky: spatializer %s, externalizer %s\n",
               atmoky_spatializer_handle ? "OK" : "missing",
               atmoky_master_externalizer ? "active" : "off");
    if (atmoky_master_externalizer)
      Com_Printf("              amount=%.0f, character=%.0f\n",
                 s_externalizer_amount.value, s_externalizer_character.value);
  }
}

/*
================
S_FMOD_Output_f

Switch FMOD output backend.
================
*/
static void S_FMOD_Output_f(void) {
  const char *arg;
  qbool found = false;
  int i;

  if (Cmd_Argc() < 2) {
    Com_Printf("Current FMOD output: %s\n", OutputTypeName(desired_output));
    Com_Printf("Usage: s_fmod_output <type>\n");
    Com_Printf("Available types:");
    for (i = 0; output_types[i].name; i++)
      Com_Printf(" %s", output_types[i].name);
    Com_Printf("\n");
    return;
  }
  arg = Cmd_Argv(1);
  for (i = 0; output_types[i].name; i++) {
    if (!Q_stricmp(arg, output_types[i].name)) {
      desired_output = output_types[i].type;
      found = true;
      break;
    }
  }
  if (!found) {
    Com_Printf("Unknown FMOD output type '%s'\nAvailable:", arg);
    for (i = 0; output_types[i].name; i++)
      Com_Printf(" %s", output_types[i].name);
    Com_Printf("\n");
    return;
  }
  Com_Printf("FMOD output set to '%s'...\n", arg);

  // TODO: Implement the actual restart here once you've verified
  // everything works.  The sequence is:
  //
  //   1. desired_output is already set above.
  //   2. S_Restart() calls S_Shutdown() then S_Init().
  //   3. S_Init() calls FMOD_System_SetOutput(desired_output)
  //      after FMOD_System_Create() but before FMOD_System_Init().
  //
  // Uncomment when ready:
  // S_Restart();
}

/*
================
S_FMOD_Drivers_f

List all available audio output drivers.
================
*/
static void S_FMOD_Drivers_f(void) {
  int ndrivers, i;
  char name[256];
  int rate, nchannels;
  FMOD_SPEAKERMODE mode;
  FMOD_RESULT result;
  if (!fmod_system) {
    Com_Printf("FMOD system not created\n");
    return;
  }
  result = FMOD_System_GetNumDrivers(fmod_system, &ndrivers);
  FMOD_ERRCHECK(result, "GetNumDrivers");
  Com_Printf("--- FMOD Audio Drivers ---\n");
  for (i = 0; i < ndrivers; i++) {
    result = FMOD_System_GetDriverInfo(fmod_system, i, name, sizeof(name), NULL,
                                       &rate, &mode, &nchannels);
    if (result == FMOD_OK)
      Com_Printf("  %d: %s (%d Hz, %d ch)\n", i, name, rate, nchannels);
    else
      Com_Printf("  %d: <error: %s>\n", i, FMOD_ErrorString(result));
  }
  Com_Printf("Total: %d driver(s)\n", ndrivers);
}

/*
================
S_FMOD_Restart_f
================
*/
static void S_FMOD_Restart_f(void) {
  Com_Printf("Restarting FMOD sound system...\n");
  S_Restart();
}

void S_ClearPrecache(void) {}
void S_BeginPrecaching(void) {}
void S_EndPrecaching(void) {}
void S_PaintChannels(int endtime) {}
void SND_InitScaletable(void) {}
channel_t *SND_PickChannel(int entnum, int entchannel) { return NULL; }
void SND_Spatialize(channel_t *ch) {}
wavinfo_t GetWavinfo(char *name, byte *wav, int wavlength) {
  wavinfo_t info;
  memset(&info, 0, sizeof(info));
  return info;
}
qbool SNDDMA_Init(void) { return false; }
int SNDDMA_GetDMAPos(void) { return 0; }
void SNDDMA_Shutdown(void) {}
void SNDDMA_Submit(void) {}

char *DSoundError(int error) { return "N/A (FMOD backend active)"; }
