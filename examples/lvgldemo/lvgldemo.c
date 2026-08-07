/****************************************************************************
 * apps/examples/lvgldemo/lvgldemo.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <unistd.h>
#include <sys/boardctl.h>

#include <lvgl/lvgl.h>
#include <lvgl/demos/lv_demos.h>

#include <fcntl.h>
#include <errno.h>
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#include <uv.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t uv_info;
  void *data;

  uv_loop_init(loop);

  lv_memset(&uv_info, 0, sizeof(uv_info));
  uv_info.loop = loop;
  uv_info.disp = result->disp;
  uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  uv_info.uindev = result->utouch_indev;
#endif

  data = lv_nuttx_uv_init(&uv_info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_EXAMPLES_LVGLDEMO_KEYPAD

/* A keypad for LVGL, reading the characters a keyboard driver hands out.
 *
 * LVGL's NuttX drivers cover a touchscreen and nothing else, so this is
 * how a board whose only input is a keyboard reaches the widgets: one
 * character per poll, translated into the keys LVGL navigates by (tab
 * moves on, shift-tab moves back, return presses), with anything else
 * passed through as itself, which is what a text field wants.
 */

static int g_keypad_fd = -1;

static void keypad_read(FAR lv_indev_t *indev, FAR lv_indev_data_t *data)
{
  char ch;

  data->state = LV_INDEV_STATE_RELEASED;

  if (g_keypad_fd < 0 || read(g_keypad_fd, &ch, 1) != 1)
    {
      return;
    }

  switch (ch)
    {
      case '\r':
      case '\n':
        data->key = LV_KEY_ENTER;
        break;

      case '\t':
        data->key = LV_KEY_NEXT;
        break;

      case 0x7f:
      case '\b':
        data->key = LV_KEY_BACKSPACE;
        break;

      case 0x1b:
        data->key = LV_KEY_ESC;
        break;

      default:
        data->key = (uint32_t)ch;
        break;
    }

  data->state = LV_INDEV_STATE_PRESSED;
}

static void keypad_create(void)
{
  FAR lv_indev_t *indev;
  FAR lv_group_t *group;

  g_keypad_fd = open(CONFIG_EXAMPLES_LVGLDEMO_KEYPAD_DEVPATH,
                     O_RDONLY | O_NONBLOCK);
  if (g_keypad_fd < 0)
    {
      LV_LOG_WARN("no keypad at %s: %d",
                  CONFIG_EXAMPLES_LVGLDEMO_KEYPAD_DEVPATH, errno);
      return;
    }

  indev = lv_indev_create();
  if (indev == NULL)
    {
      close(g_keypad_fd);
      g_keypad_fd = -1;
      return;
    }

  lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(indev, keypad_read);

  /* Widgets only receive keys through a group, and a demo builds its
   * own widgets without making one, so the default group is created
   * here and handed to the keypad.
   */

  group = lv_group_create();
  lv_group_set_default(group);
  lv_indev_set_group(indev, group);

  LV_LOG_USER("keypad on %s", CONFIG_EXAMPLES_LVGLDEMO_KEYPAD_DEVPATH);
}
#endif

/****************************************************************************
 * Name: main or lv_demos_main
 *
 * Description:
 *
 * Input Parameters:
 *   Standard argc and argv
 *
 * Returned Value:
 *   Zero on success; a positive, non-zero value on failure.
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  uv_loop_t ui_loop;
  lv_memzero(&ui_loop, sizeof(ui_loop));
#endif

  if (lv_is_initialized())
    {
      LV_LOG_ERROR("LVGL already initialized! aborting.");
      return -1;
    }

  lv_init();

  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#endif

  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      LV_LOG_ERROR("lv_demos initialization failure!");
      return 1;
    }

#ifdef CONFIG_EXAMPLES_LVGLDEMO_KEYPAD
  keypad_create();
#endif

  if (!lv_demos_create(&argv[1], argc - 1))
    {
      lv_demos_show_help();

      /* we can add custom demos here */

      goto demo_end;
    }

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  lv_nuttx_uv_loop(&ui_loop, &result);
#else
  while (1)
    {
      uint32_t idle;
      idle = lv_timer_handler();

      /* Minimum sleep of 1ms */

      idle = idle ? idle : 1;
      usleep(idle * 1000);
    }
#endif

demo_end:
  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
