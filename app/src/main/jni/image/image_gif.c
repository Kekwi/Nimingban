/*
 * Copyright 2015 Hippo Seven
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by Hippo on 12/27/2015.
//

#include "config.h"
#ifdef IMAGE_SUPPORT_GIF

#include <stdlib.h>

#include "image_gif.h"
#include "image_utils.h"
#include "java_wrapper.h"
#include "../log.h"
#include "../utils.h"

static int error_code = 0;

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
  unsigned char alpha;
} RGBA;

static int custom_read_fun(GifFileType* gif, GifByteType* bytes, int size) {
  PatchHeadInputStream* patch_head_input_stream = gif->UserData;
  JNIEnv *env = get_env();

  if (env == NULL) {
    LOGE(EMSG("Can't get JNIEnv"));
    return 0;
  }

  return read_patch_head_input_stream(env, patch_head_input_stream, bytes, 0, size);
}

static void generate_prepare(GIF_FRAME_INFO* frame_info_array, int count)
{
  int disposal = 0xff;
  int i;

  for (i = 0; i < count; i++){
    GIF_FRAME_INFO* frame_info = frame_info_array + i;

    switch (disposal) {
      case DISPOSAL_UNSPECIFIED:
      case DISPOSE_DO_NOT:
        frame_info->prepare = IMAGE_GIF_PREPARE_NONE;
        break;
      case DISPOSE_BACKGROUND:
        frame_info->prepare = IMAGE_GIF_PREPARE_BACKGROUND;
        break;
      case DISPOSE_PREVIOUS:
        frame_info->prepare = IMAGE_GIF_PREPARE_USE_BACKUP;
        break;
      default:
        frame_info->prepare = IMAGE_GIF_PREPARE_BACKGROUND;
        break;
    }

    disposal = frame_info->disposal;
  }
}

static void read_gcb(GifFileType* gif_file, int index, GIF_FRAME_INFO* frame_info)
{
  GraphicsControlBlock gcb;
  if (DGifSavedExtensionToGCB(gif_file, index, &gcb) == GIF_OK) {
    frame_info->tran = gcb.TransparentColor;
    frame_info->delay = gcb.DelayTime * 10;
    frame_info->disposal = gcb.DisposalMode;
    frame_info->prepare = IMAGE_GIF_PREPARE_UNKNOWN;
  } else {
    frame_info->tran = -1;
    frame_info->delay = 0;
    frame_info->disposal = DISPOSE_DO_NOT;
    frame_info->prepare = IMAGE_GIF_PREPARE_UNKNOWN;
  }
}

void* GIF_decode(JNIEnv* env, PatchHeadInputStream* patch_head_input_stream, bool partially)
{
  GIF* gif = NULL;
  GifFileType* gif_file = NULL;
  void* buffer = NULL;
  GIF_FRAME_INFO* frame_info_array = NULL;
  int i;

  gif = (GIF*) malloc(sizeof(GIF));
  if (gif == NULL) {
    WTF_OM;
    close_patch_head_input_stream(get_env(), patch_head_input_stream);
    destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
    return NULL;
  }

  // Open
  gif_file = DGifOpen(patch_head_input_stream, &custom_read_fun, &error_code);
  if (gif_file == NULL) {
    WTF_OM;
    free(gif);
    close_patch_head_input_stream(get_env(), patch_head_input_stream);
    destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
    return NULL;
  }

  // Buffer
  buffer = malloc(gif_file->SWidth * gif_file->SHeight * sizeof(RGBA));
  if (buffer == NULL) {
    WTF_OM;
    DGifCloseFile(gif_file, &error_code);
    free(gif);
    close_patch_head_input_stream(get_env(), patch_head_input_stream);
    destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
    return NULL;
  }

  if (partially) {
    // Glance
    if (DGifGlance(gif_file) != GIF_OK) {
      LOGE(EMSG("GIF error code %d"), error_code);
      DGifCloseFile(gif_file, &error_code);
      free(buffer);
      free(gif);
      close_patch_head_input_stream(get_env(), patch_head_input_stream);
      destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
      return NULL;
    }

    // Frame info
    frame_info_array = (GIF_FRAME_INFO*) malloc(sizeof(GIF_FRAME_INFO));
    if (frame_info_array == NULL) {
      WTF_OM;
      DGifCloseFile(gif_file, &error_code);
      free(buffer);
      free(gif);
      close_patch_head_input_stream(get_env(), patch_head_input_stream);
      destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
      return NULL;
    }

    // Read gcb
    read_gcb(gif_file, 0, frame_info_array);

    gif->partially = true;
    gif->patch_head_input_stream = patch_head_input_stream;
  } else {
    // Slurp
    DGifSlurp(gif_file);
    if (gif_file->ImageCount <= 0) {
      LOGE(EMSG("No frame"));
      DGifCloseFile(gif_file, &error_code);
      free(buffer);
      free(gif);
      close_patch_head_input_stream(get_env(), patch_head_input_stream);
      destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
      return NULL;
    }

    // Frame info
    frame_info_array = (GIF_FRAME_INFO*) malloc(gif_file->ImageCount * sizeof(GIF_FRAME_INFO));
    if (frame_info_array == NULL) {
      WTF_OM;
      DGifCloseFile(gif_file, &error_code);
      free(buffer);
      free(gif);
      close_patch_head_input_stream(get_env(), patch_head_input_stream);
      destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);
      return NULL;
    }

    // Read gcb
    for (i = 0; i < gif_file->ImageCount; i++) {
      read_gcb(gif_file, i, frame_info_array + i);
    }

    // Generate prepare
    generate_prepare(frame_info_array, gif_file->ImageCount);

    // Close input stream
    close_patch_head_input_stream(get_env(), patch_head_input_stream);
    destroy_patch_head_input_stream(get_env(), &patch_head_input_stream);

    gif->partially = false;
    gif->patch_head_input_stream = NULL;
  }

  gif->gif_file = gif_file;
  gif->frame_info_array = frame_info_array;
  gif->buffer = buffer;
  gif->buffer_index = -1;
  gif->backup = NULL;

  GIF_advance(gif);

  return gif;
}

bool GIF_complete(GIF* gif)
{
  int i;

  if (!gif->partially) {
    return true;
  }

  if (gif->gif_file == NULL || gif->patch_head_input_stream == NULL) {
    LOGE(EMSG("Some stuff is NULL"));
    return false;
  }

  DGifSlurp(gif->gif_file);

  // Close input stream
  close_patch_head_input_stream(get_env(), gif->patch_head_input_stream);
  destroy_patch_head_input_stream(get_env(), &gif->patch_head_input_stream);
  gif->patch_head_input_stream = NULL;

  gif->partially = false;

  gif->frame_info_array = (GIF_FRAME_INFO*) realloc(gif->frame_info_array,
      gif->gif_file->ImageCount * sizeof(GIF_FRAME_INFO));
  if (gif->frame_info_array == NULL) {
    WTF_OM;
    return false;
  }

  // Read gcb
  for (i = 0; i < gif->gif_file->ImageCount; i++) {
    read_gcb(gif->gif_file, i, gif->frame_info_array + i);
  }

  // Generate prepare
  generate_prepare(gif->frame_info_array, gif->gif_file->ImageCount);

  return true;
}

bool GIF_is_completed(GIF* gif)
{
  return !gif->partially;
}

int GIF_get_width(GIF* gif)
{
  return gif->gif_file->SWidth;
}

int GIF_get_height(GIF* gif)
{
  return gif->gif_file->SHeight;
}

bool GIF_render(GIF* gif, int src_x, int src_y,
    void* dst, int dst_w, int dst_h, int dst_x, int dst_y,
    int width, int height, bool fill_blank, int default_color)
{
  return copy_pixels(gif->buffer, gif->gif_file->SWidth, gif->gif_file->SHeight, src_x, src_y,
      dst, dst_w, dst_h, dst_x, dst_y,
      width, height, fill_blank, default_color);
}

static void backup(GIF* gif)
{
  if (gif->backup == NULL) {
    gif->backup = (unsigned char*) malloc((size_t) (gif->gif_file->SWidth * gif->gif_file->SHeight * 4));
    if (gif->backup == NULL) {
      WTF_OM;
      return;
    }
  }
  memcpy(gif->backup, gif->buffer, (size_t) (gif->gif_file->SWidth * gif->gif_file->SHeight * 4));
}

static void restore(GIF* gif)
{
  if (gif->backup == NULL) {
    LOGE(EMSG("Backup is NULL"));
  } else {
    memcpy(gif->buffer, gif->backup, (size_t) (gif->gif_file->SWidth * gif->gif_file->SHeight * 4));
  }
}

static void switch_buffer_backup(GIF* gif)
{
  unsigned char* temp;

  if (gif->backup == NULL) {
    backup(gif);
  } else {
    temp = gif->buffer;
    gif->buffer = gif->backup;
    gif->backup = temp;
  }
}

static void clear(RGBA* pixels, int num, RGBA color) {
  RGBA* ptr = pixels;
  int i;
  for (i = 0; i < num; i++) {
    *(ptr++) = color;
  }
}

static bool get_color_from_table(const ColorMapObject* cmap, int index,
    RGBA* color) {
  if (cmap == NULL || index < 0 || index >= cmap->ColorCount) {
    return false;
  } else {
    GifColorType gct = cmap->Colors[index];
    color->red = gct.Red;
    color->green = gct.Green;
    color->blue = gct.Blue;
    color->alpha = 0xff;
    return true;
  }
}

static void clear_bg(GifFileType* gif_file, void* pixels)
{
  RGBA color;
  if (!get_color_from_table(gif_file->SColorMap, gif_file->SBackGroundColor, &color)) {
    color.red = 0x00;
    color.green = 0x00;
    color.blue = 0x00;
    color.alpha = 0x00;
  }
  clear(pixels, gif_file->SWidth * gif_file->SHeight, color);
}

static void copy_line(GifByteType* src, RGBA* dst,
    const ColorMapObject* cmap, int tran, int len)
{
  for (; len > 0; len--, src++, dst++) {
    int index = *src;
    if (tran == -1 || index != tran) {
      get_color_from_table(cmap, index, dst);
    }
  }
}

static void blend(GifFileType* gif_file, int index, void* pixels, int tran)
{
  int width = gif_file->SWidth;
  int height = gif_file->SHeight;
  SavedImage cur = gif_file->SavedImages[index];
  GifImageDesc desc = cur.ImageDesc;
  int copy_width = MIN(width - desc.Left, desc.Width);
  int copy_height = MIN(height - desc.Top, desc.Height);
  ColorMapObject *cmap = desc.ColorMap;
  GifByteType* src = cur.RasterBits;
  RGBA* dst = pixels;
  GifByteType* src_ptr;
  RGBA* dst_ptr;
  size_t len;
  int i;

  if (cmap == NULL) {
    cmap = gif_file->SColorMap;
  }
  if (cmap != NULL) {
    for (i = 0; i < copy_height; i++) {
      src_ptr = src + (i * desc.Width);
      dst_ptr = dst + ((desc.Top + i) * width + desc.Left);
      len = (size_t) copy_width;
      copy_line(src_ptr, dst_ptr, cmap, tran, len);
    }
  } else {
    LOGW(EMSG("Can't find color map"));
  }
}

void GIF_advance(GIF* gif)
{
  int index;
  GIF_FRAME_INFO frame_info;

  index = (gif->buffer_index + 1) % gif->gif_file->ImageCount;
  if (index != 0 && gif->partially) {
    LOGE(EMSG("The png is only decoded partially. Only the first frame can be shown."));
    return;
  }

  if (gif->frame_info_array == NULL) {
    frame_info.tran = -1;
    frame_info.disposal = DISPOSE_DO_NOT;
    frame_info.delay = 0;
    frame_info.prepare = IMAGE_GIF_PREPARE_BACKGROUND;
  } else {
    memcpy(&frame_info, gif->frame_info_array + index, sizeof(GIF_FRAME_INFO));
  }

  if (frame_info.disposal == DISPOSE_PREVIOUS && frame_info.prepare == IMAGE_GIF_PREPARE_USE_BACKUP) {
    switch_buffer_backup(gif);
  } else {
    // Backup
    if (frame_info.disposal == DISPOSE_PREVIOUS) {
      backup(gif);
    }

    // Prepare
    switch (frame_info.prepare) {
      case IMAGE_GIF_PREPARE_NONE:
        // Do nothing
        break;
      default:
      case IMAGE_GIF_PREPARE_BACKGROUND:
        // Set bg
        clear_bg(gif->gif_file, gif->buffer);
        break;
      case IMAGE_GIF_PREPARE_USE_BACKUP:
        restore(gif);
        break;
    }
  }

  blend(gif->gif_file, index, gif->buffer, frame_info.tran);

  gif->buffer_index = index;
}

int GIF_get_delay(GIF* gif)
{
  if (gif->frame_info_array == NULL) {
    return 0;
  } else {
    return gif->frame_info_array[gif->buffer_index].delay;
  }
}

int GIF_get_frame_count(GIF* gif)
{
  return gif->gif_file->ImageCount;
}

void GIF_recycle(GIF* gif)
{
  DGifCloseFile(gif->gif_file, &error_code);
  gif->gif_file = NULL;

  free(gif->frame_info_array);
  gif->frame_info_array = NULL;

  free(gif->buffer);
  gif->buffer = NULL;

  free(gif->backup);
  gif->backup = NULL;

  if (gif->patch_head_input_stream != NULL) {
    close_patch_head_input_stream(get_env(), gif->patch_head_input_stream);
    destroy_patch_head_input_stream(get_env(), &gif->patch_head_input_stream);
    gif->patch_head_input_stream = NULL;
  }
}

#endif // IMAGE_SUPPORT_GIF
