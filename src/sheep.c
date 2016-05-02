/*
  Sheep for Pebble-C
  License: WTFPL
 */

#include <pebble.h>
#include "sheep.h"

#define FPS 10

#define TRUE 1
#define FALSE 0

static Window *window;

static AppTimer *progress_timer;

static unsigned int sleep_time = FPS*10;

static GBitmap *bg_image;
static BitmapLayer *bg_image_layer;

static GBitmap *sheep00_image;
static GBitmap *sheep01_image;

#define SHEEP00_WIDTH 17
#define SHEEP00_HEIGHT 12
#define SHEEP01_WIDTH 16
#define SHEEP01_HEIGHT 12

static Layer *canvas_layer;

static TextLayer *text_layer;

static void progress_timer_callback(void *data);

static int sheep_count = 0;
static char sheep_count_buffer[256];
static char *nofsheep = " sheep";

static int some_sheep_is_running=FALSE;

#define DEFAULT_WIDTH 144
#define DEFAULT_HEIGHT 144
static int screen_width = DEFAULT_WIDTH;
static int screen_height = DEFAULT_HEIGHT;

#define FENCE_IMG_WIDTH 52
#define FENCE_IMG_HEIGHT 78

#define POLL_HEIGHT 12
#define POLL_WIDTH 8

static int fence_width = FENCE_IMG_WIDTH - POLL_WIDTH;
static int fence_height = FENCE_IMG_HEIGHT - POLL_HEIGHT;

#define X_MOVING_DIST 5
#define Y_MOVING_DIST 3

#define GROUND_HEIGHT_RATIO 0.9
static int ground_height = 70;

static int gate_is_widely_open = FALSE;

#define MAX_SHEEP_NUMBER 100

enum Sheep_Attr {
  PHASE,
  X,
  Y,
  COUNT_ON_JUMP,
  X_ON_JUMP,
  X_AT_TOP_ON_JUMP,
  STRETCH_LEG
};

enum Sheep_Phase {
  ENTER_RUN,
  JUMP_UP,
  JUMP_DOWN,
  EXIT_RUN,
  REST
};

int sheep_flock[MAX_SHEEP_NUMBER][STRETCH_LEG+1];

/* simple base 10 only itoa */
char *
mknofsheep (int value, char *unit, char *result)
{
  char const digit[] = "0123456789";
  char *p = result;
  if (value < 0) {
    *p++ = '-';
    value *= -1;
  }

  /* move number of required chars and null terminate */
  int shift = value;
  do {
    ++p;
    shift /= 10;
  } while (shift);
  *p = '\0';

  /* populate result in reverse order */
  do {
    *--p = digit [value % 10];
    value /= 10;
  } while (value);

  strcat(result, unit);

  return result;
}

static void canvas_update_proc(Layer *this_layer, GContext *ctx) {

//  graphics_context_set_compositing_mode(ctx, GCompOpClear);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  // Draw the sheep
  for (int asheep=0;asheep<MAX_SHEEP_NUMBER;asheep++){
    if(sheep_flock[asheep][PHASE]!=REST){
      if(sheep_flock[asheep][COUNT_ON_JUMP]>0){
        // graphics_draw_bitmap_in_rect(ctx, sheep01_image,
        //                             GRect(sheep_flock[asheep][X],sheep_flock[asheep][Y],17,12));
        graphics_draw_bitmap_in_rect(ctx, sheep01_image, GRect(sheep_flock[asheep][X],sheep_flock[asheep][Y]-SHEEP01_HEIGHT,SHEEP01_WIDTH,SHEEP01_HEIGHT));
     } else {
        if(sheep_flock[asheep][STRETCH_LEG]==TRUE){
          graphics_draw_bitmap_in_rect(ctx, sheep01_image, GRect(sheep_flock[asheep][X], sheep_flock[asheep][Y]-SHEEP01_HEIGHT,SHEEP01_WIDTH,SHEEP01_HEIGHT));
        } else {
          graphics_draw_bitmap_in_rect(ctx, sheep00_image, GRect(sheep_flock[asheep][X], sheep_flock[asheep][Y]-SHEEP00_HEIGHT,SHEEP00_WIDTH,SHEEP00_HEIGHT));
        }
      }
    }
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  GRect window_bounds = layer_get_bounds(window_layer);

  screen_width = window_bounds.size.w;
  screen_height = window_bounds.size.h;

  // We do this to account for the offset due to the status bar
  // at the top of the app window.
  GRect image_frame = layer_get_frame(window_layer);
  image_frame.origin.x = 0;
  image_frame.origin.y = 0;

  bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BG);

  sheep00_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SHEEP00);

  sheep01_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SHEEP01);

  // Use GCompOpOr to display the white portions of the fence image
  bg_image_layer = bitmap_layer_create(image_frame);
  bitmap_layer_set_alignment(bg_image_layer, GAlignBottom);
  bitmap_layer_set_bitmap(bg_image_layer, bg_image);
  bitmap_layer_set_compositing_mode(bg_image_layer, GCompOpAssign);
  layer_add_child(window_layer, bitmap_layer_get_layer(bg_image_layer));

  // Create Layer
  canvas_layer = layer_create(GRect(0, 0, image_frame.size.w, image_frame.size.h));

  layer_add_child(window_layer, canvas_layer);

  // Set the update_proc
  layer_set_update_proc(canvas_layer, canvas_update_proc);

  if(window_bounds.size.w==180){ // for Chalk
    text_layer = text_layer_create(GRect(55,15, 144, 15));
  } else {
   text_layer = text_layer_create(GRect(0,0, 144, 15));
  }
  text_layer_set_background_color(text_layer, GColorFromRGB(150,150,255));
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void send_out_sheep(int asheep){
  sheep_flock[asheep][PHASE] = ENTER_RUN;
  sheep_flock[asheep][STRETCH_LEG] = FALSE;
  sheep_flock[asheep][X] = screen_width + 17;

  sheep_flock[asheep][Y] = screen_height-5 - (rand()%(ground_height-10));

  int rightside_x_of_first_poll = screen_width/2 - FENCE_IMG_WIDTH/2 + POLL_WIDTH;

//  int dist_on_jump_from_poll = POLL_HEIGHT * X_MOVING_DIST / Y_MOVING_DIST - 4;
  int dist_on_jump_from_poll = (POLL_HEIGHT * X_MOVING_DIST / Y_MOVING_DIST)/2;

  int x_offset = (screen_height - sheep_flock[asheep][Y]) * fence_width / fence_height;

//  sheep_flock[asheep][X_ON_JUMP] = rightside_x_of_first_poll + dist_on_jump_from_poll + x_offset;
  sheep_flock[asheep][X_ON_JUMP] = rightside_x_of_first_poll + dist_on_jump_from_poll + x_offset;

//  sheep_flock[asheep][X_AT_TOP_ON_JUMP] = sheep_flock[asheep][X_ON_JUMP] - fence_width / 2 + x_offset;
  sheep_flock[asheep][X_AT_TOP_ON_JUMP] = rightside_x_of_first_poll - POLL_WIDTH / 2 + x_offset;
/*
  printf("calc_jump_x: X:%d, Y:%d", sheep_flock[asheep][X], sheep_flock[asheep][Y]);
  printf("calc_jump_x: rightside_x_of_first_poll:%d, dist_on_jump_from_poll:%d", rightside_x_of_first_poll, dist_on_jump_from_poll);
  printf("calc_jump_x: y_offset:%d, X_ON_JUMP:%d, X_AT_TOP_ON_JUMP:%d", x_offset, sheep_flock[asheep][X_ON_JUMP], sheep_flock[asheep][X_AT_TOP_ON_JUMP]);  
*/
  // printf("send_out: y=%d, jump_x=%d", sheep_flock[asheep][Y],sheep_flock[asheep][X_ON_JUMP]);
}

static void clear_sheep(int asheep){
  sheep_flock[asheep][PHASE] = REST;
  sheep_flock[asheep][X] = 0;
  sheep_flock[asheep][Y] = 0;
  sheep_flock[asheep][COUNT_ON_JUMP] = 0;
  sheep_flock[asheep][X_ON_JUMP] = 0;
  sheep_flock[asheep][STRETCH_LEG] = FALSE;
}

static void update() {

  // Send out a sheep
  if (gate_is_widely_open==TRUE) {
    for (int i=0; i<MAX_SHEEP_NUMBER; i++){
      if(sheep_flock[i][PHASE] == REST){
        send_out_sheep(i);
        break;
      }
    }
  }

  // Update status for each sheep
  for(int asheep=0;asheep<MAX_SHEEP_NUMBER;asheep++){
    if (sheep_flock[asheep][PHASE]==REST){
        continue;
    }

    // Run
    sheep_flock[asheep][X] -= X_MOVING_DIST;

    switch (sheep_flock[asheep][PHASE]) {
      case ENTER_RUN:
        // printf("enter run, X_ON_JUMP:%d, x:%d, y:%d", sheep_flock[asheep][X_ON_JUMP], sheep_flock[asheep][X], sheep_flock[asheep][Y]);
        if(sheep_flock[asheep][X_ON_JUMP]>=sheep_flock[asheep][X]){
          // printf("sheep start to jump! X_ON_JUMP:%d, x:%d", sheep_flock[asheep][X_ON_JUMP], sheep_flock[asheep][X]);
          sheep_flock[asheep][PHASE]=JUMP_UP;
          sheep_flock[asheep][Y] -= Y_MOVING_DIST;
          sheep_flock[asheep][COUNT_ON_JUMP] = 1;
        }
        break;
      case JUMP_UP:
        // printf("jump_up, x:%d, y:%d", sheep_flock[asheep][X], sheep_flock[asheep][Y]);
        // count up and start to jump down
        if (sheep_flock[asheep][X_AT_TOP_ON_JUMP]>(sheep_flock[asheep][X]+16/2)){
          sheep_flock[asheep][PHASE]=JUMP_DOWN;
          sheep_flock[asheep][Y] += Y_MOVING_DIST;
          sheep_flock[asheep][COUNT_ON_JUMP] -=1;
          if (sheep_count < 999999){
            sheep_count += 1;
          } else {
            sheep_count = 0;
          }
          break;
        }
        sheep_flock[asheep][Y] -= Y_MOVING_DIST;
        sheep_flock[asheep][COUNT_ON_JUMP] += 1;
        break;
      case JUMP_DOWN:
        // printf("jump_down, x:%d, y:%d", sheep_flock[asheep][X], sheep_flock[asheep][Y]);
        if(sheep_flock[asheep][COUNT_ON_JUMP]<=0){
          sheep_flock[asheep][PHASE]=EXIT_RUN;
          // printf("sheep end to jump! %d, %d", sheep_flock[asheep][X_ON_JUMP], sheep_flock[asheep][X]);
          break;
        }
        sheep_flock[asheep][Y] += Y_MOVING_DIST;
        sheep_flock[asheep][COUNT_ON_JUMP] -= 1;
        break;
      case EXIT_RUN:
        printf("exit run, X_ON_JUMP:%d, x:%d, y:%d", sheep_flock[asheep][X_ON_JUMP], sheep_flock[asheep][X], sheep_flock[asheep][Y]);
        break;
      default:
        break;
    }

    // go away and send out a sheep if there is no sheep on run
    if (sheep_flock[asheep][X] < -1 * 17){
      some_sheep_is_running = FALSE;
      clear_sheep(asheep);
      for (int asheep=0;asheep<MAX_SHEEP_NUMBER;asheep++) {
        if (sheep_flock[asheep][PHASE]!=REST) {
          some_sheep_is_running = TRUE;
          break;
        }
      }
      if (some_sheep_is_running == FALSE) {
        send_out_sheep(asheep);
      }
    }
  }

  mknofsheep(sheep_count, nofsheep, sheep_count_buffer);
}


static void draw() {
  text_layer_set_text(text_layer, sheep_count_buffer);
}

static void progress_timer_callback(void *data) {
  progress_timer = app_timer_register(sleep_time /* milliseconds */, progress_timer_callback, NULL);
  update();
  draw();
}

static void window_unload(Window *window) {
  bitmap_layer_destroy(bg_image_layer);

  layer_destroy(canvas_layer);
  text_layer_destroy(text_layer);

  gbitmap_destroy(bg_image);
  gbitmap_destroy(sheep00_image);
  gbitmap_destroy(sheep01_image);
}

void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  gate_is_widely_open=TRUE;
}
void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  for (int asheep=0;asheep<MAX_SHEEP_NUMBER;asheep++) {
    if (sheep_flock[asheep][PHASE]==REST) {
      send_out_sheep(asheep);
      break;
    }
  }
}
void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  gate_is_widely_open=FALSE;
}


void config_provider(Window *window) {
  // single click config:
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
}
  
static void init(void) {
  for (int asheep=0; asheep<MAX_SHEEP_NUMBER; asheep++){
    clear_sheep(asheep);
  }

  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, false);

  window_set_click_config_provider(window, (ClickConfigProvider) config_provider);

  progress_timer = app_timer_register(sleep_time /* milliseconds */, progress_timer_callback, NULL);

  send_out_sheep(0);
}

static void deinit(void) {
  window_destroy(window);

}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

