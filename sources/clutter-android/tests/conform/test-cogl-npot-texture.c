#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

/* Non-power-of-two sized texture that should cause slicing */
#define TEXTURE_SIZE        384
/* Number of times to split the texture up on each axis */
#define PARTS               2
/* The texture is split into four parts, each with a different colour */
#define PART_SIZE           (TEXTURE_SIZE / PARTS)

/* Amount of pixels to skip off the top, bottom, left and right of the
   texture when reading back the stage */
#define TEST_INSET          4

/* Size to actually render the texture at */
#define TEXTURE_RENDER_SIZE TEXTURE_SIZE
/* The size of a part once rendered */
#define PART_RENDER_SIZE    (TEXTURE_RENDER_SIZE / PARTS)

static const ClutterColor corner_colors[PARTS * PARTS] =
  {
    /* Top left     - red */    { 255, 0,   0,   255 },
    /* Top right    - green */  { 0,   255, 0,   255 },
    /* Bottom left  - blue */   { 0,   0,   255, 255 },
    /* Bottom right - yellow */ { 255, 255, 0,   255 }
  };

typedef struct _TestState
{
  ClutterActor *stage;
  guint frame;
  CoglHandle texture;
} TestState;

static gboolean
validate_part (ClutterActor *stage,
               int xnum,
               int ynum,
               const ClutterColor *color)
{
  guchar *pixels, *p;
  gboolean ret = TRUE;

  /* Read the appropriate part but skip out a few pixels around the
     edges */
  pixels = clutter_stage_read_pixels (CLUTTER_STAGE (stage),
                                      xnum * PART_RENDER_SIZE + TEST_INSET,
                                      ynum * PART_RENDER_SIZE + TEST_INSET,
                                      PART_RENDER_SIZE - TEST_INSET * 2,
                                      PART_RENDER_SIZE - TEST_INSET * 2);

  /* Make sure every pixels is the appropriate color */
  for (p = pixels;
       p < pixels + ((PART_RENDER_SIZE - TEST_INSET * 2)
                     * (PART_RENDER_SIZE - TEST_INSET * 2));
       p += 4)
    {
      if (p[0] != color->red)
        ret = FALSE;
      if (p[1] != color->green)
        ret = FALSE;
      if (p[2] != color->blue)
        ret = FALSE;
    }

  g_free (pixels);

  return ret;
}

static void
validate_result (TestState *state)
{
  /* Validate that all four corners of the texture are drawn in the
     right color */
  g_assert (validate_part (state->stage, 0, 0, corner_colors + 0));
  g_assert (validate_part (state->stage, 1, 0, corner_colors + 1));
  g_assert (validate_part (state->stage, 0, 1, corner_colors + 2));
  g_assert (validate_part (state->stage, 1, 1, corner_colors + 3));

  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int frame_num;
  int y, x;

  /* Just render the texture in the top left corner */
  cogl_set_source_texture (state->texture);
  /* Render the texture using four separate rectangles */
  for (y = 0; y < 2; y++)
    for (x = 0; x < 2; x++)
      cogl_rectangle_with_texture_coords (x * TEXTURE_RENDER_SIZE / 2,
                                          y * TEXTURE_RENDER_SIZE / 2,
                                          (x + 1) * TEXTURE_RENDER_SIZE / 2,
                                          (y + 1) * TEXTURE_RENDER_SIZE / 2,
                                          x / 2.0f,
                                          y / 2.0f,
                                          (x + 1) / 2.0f,
                                          (y + 1) / 2.0f);

  /* XXX: validate_result calls clutter_stage_read_pixels which will result in
   * another paint run so to avoid infinite recursion we only aim to validate
   * the first frame. */
  frame_num = state->frame++;
  if (frame_num == 1)
    validate_result (state);
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return G_SOURCE_CONTINUE;
}

static CoglHandle
make_texture (void)
{
  guchar *tex_data, *p;
  CoglHandle tex;
  int partx, party, width, height;

  p = tex_data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  /* Make a texture with a different color for each part */
  for (party = 0; party < PARTS; party++)
    {
      height = (party < PARTS - 1
                ? PART_SIZE
                : TEXTURE_SIZE - PART_SIZE * (PARTS - 1));

      for (partx = 0; partx < PARTS; partx++)
        {
          const ClutterColor *color = corner_colors + party * PARTS + partx;
          width = (partx < PARTS - 1
                   ? PART_SIZE
                   : TEXTURE_SIZE - PART_SIZE * (PARTS - 1));

          while (width-- > 0)
            {
              *(p++) = color->red;
              *(p++) = color->green;
              *(p++) = color->blue;
              *(p++) = color->alpha;
            }
        }

      while (--height > 0)
        {
          memcpy (p, p - TEXTURE_SIZE * 4, TEXTURE_SIZE * 4);
          p += TEXTURE_SIZE * 4;
        }
    }

  tex = cogl_texture_new_from_data (TEXTURE_SIZE,
                                    TEXTURE_SIZE,
                                    COGL_TEXTURE_NO_ATLAS,
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEXTURE_SIZE * 4,
                                    tex_data);

  g_free (tex_data);

  if (g_test_verbose ())
    {
      if (cogl_texture_is_sliced (tex))
        g_print ("Texture is sliced\n");
      else
        g_print ("Texture is not sliced\n");
    }

  /* The texture should be sliced unless NPOTs are supported */
  g_assert (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT)
            ? !cogl_texture_is_sliced (tex)
            : cogl_texture_is_sliced (tex));

  return tex;
}

void
test_cogl_npot_texture (TestConformSimpleFixture *fixture,
                        gconstpointer data)
{
  TestState state;
  ClutterActor *stage;
  ClutterActor *group;
  guint idle_source;

  if (g_test_verbose ())
    {
      if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT))
        g_print ("NPOT textures are supported\n");
      else
        g_print ("NPOT textures are not supported\n");
    }

  state.frame = 0;

  state.texture = make_texture ();

  state.stage = stage = clutter_stage_new ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = clutter_threads_add_idle (queue_redraw, stage);

  g_signal_connect (group, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (stage);

  clutter_main ();

  g_source_remove (idle_source);

  cogl_handle_unref (state.texture);

  clutter_actor_destroy (state.stage);

  if (g_test_verbose ())
    g_print ("OK\n");
}
