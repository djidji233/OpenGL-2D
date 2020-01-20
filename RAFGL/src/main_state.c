#include <main_state.h>
#include <glad/glad.h>
#include <math.h>
#include <stdbool.h>

#include <rafgl.h>

#include <game_constants.h>

static rafgl_raster_t doge;
static rafgl_raster_t upscaled_doge;
static rafgl_raster_t raster, raster2;
static rafgl_raster_t checker;
static rafgl_raster_t gameover;
static rafgl_raster_t gameover2;
static rafgl_raster_t startraster;
int gameover_y=RASTER_HEIGHT;
static rafgl_raster_t perlin;
static rafgl_raster_t tmp;
static rafgl_raster_t winraster;


static rafgl_texture_t texture;


#define NUMBER_OF_TILES 20
rafgl_raster_t tiles[NUMBER_OF_TILES];


#define WORLD_SIZE 128
int tile_world[WORLD_SIZE][WORLD_SIZE];

#define TILE_SIZE 64

static int raster_width = RASTER_WIDTH, raster_height = RASTER_HEIGHT;
static rafgl_spritesheet_t hero;
static rafgl_spritesheet_t fire;
static char save_file[256];
int save_file_no = 0;

int camx = 600, camy = 600;

int selected_x, selected_y;

rafgl_pixel_rgb_t circle_colour, background_colour;
int radius = RASTER_WIDTH;
int svemir_life=0;
rafgl_pixel_rgb_t perlinpix,gameoverpix,gameoverpix2;
int animation_running = 0;
int animation_frame = 0;
int direction = 0;
int hover_frames = 0;

int hover_fire_frames = 0;
int animation_frame_fire = 0;
int direction_fire = 0;
int fire_life=0;
int delta_brightness=1;
int grayscale_radius=250;

int hero_pos_x = RASTER_WIDTH / 2;
int hero_pos_y = RASTER_HEIGHT / 2;
int hero_speed = 150;
int drunk=0;
int svemir=0;
int win=0;
int left=RASTER_WIDTH/2-32,right=RASTER_WIDTH/2+32,top=RASTER_HEIGHT/2-32,bottom=RASTER_HEIGHT/2+32;
int start=0;
rafgl_pixel_rgb_t sampled, sampled2, resulting, resulting2;
int win_x = 0;


	/** tile world*/
void init_tilemap(void)
{
    int x, y;

    for(y = 0; y < WORLD_SIZE; y++)
    {
        for(x = 0; x < WORLD_SIZE; x++)
        {
            if(randf() > 0.8f)
            {
                tile_world[y][x] = 3 + rand() % 5;
            }
            else
            {
                tile_world[y][x] = rand() % 5;
            }

        }
    }
    tile_world[0][0]=19;
}

void render_tilemap(rafgl_raster_t *raster)
{
    int x, y;
    int x0 = camx / TILE_SIZE;
    int x1 = x0 + (raster_width / TILE_SIZE) + 1;
    int y0 = camy / TILE_SIZE;
    int y1 = y0 + (raster_height / TILE_SIZE) + 2;

    if(x0 < 0) x0 = 0;
    if(y0 < 0) y0 = 0;
    if(x1 < 0) x1 = 0;
    if(y1 < 0) y1 = 0;

    if(x0 >= WORLD_SIZE) x0 = WORLD_SIZE - 1;
    if(y0 >= WORLD_SIZE) y0 = WORLD_SIZE - 1;
    if(x1 >= WORLD_SIZE) x1 = WORLD_SIZE - 1;
    if(y1 >= WORLD_SIZE) y1 = WORLD_SIZE - 1;

    rafgl_raster_t *draw_tile;

    for(y = y0; y <= y1; y++)
    {
        for(x = x0; x <= x1; x++)
        {
            draw_tile = tiles + (tile_world[y][x] % NUMBER_OF_TILES);
            rafgl_raster_draw_raster(raster, draw_tile, x * TILE_SIZE - camx, y * TILE_SIZE - camy - draw_tile->height + TILE_SIZE);
        }
    }

    rafgl_raster_draw_rectangle(raster, selected_x * TILE_SIZE - camx, selected_y * TILE_SIZE - camy, TILE_SIZE, TILE_SIZE, rafgl_RGB(255, 255, 0));



}

	/** perlin noise */

float cosine_interpolationf(float a, float b, float s)
{
    float f = (1.0f - cosf(s * M_PI))* 0.5f;
    return a + (b - a) * f;
}

/* slicno kao bilinearno semplovanje */
void cosine_float_map_rescale(float *dst, int dst_width, int dst_height, float *src, int src_width, int src_height)
{
    int x, y;
    float xn, yn;
    float fxs, fys;
    int ixs0, iys0;
    int ixs1, iys1;
    float upper_middle, lower_middle;
    float sample_left, sample_right;
    float result;

    for(y = 0; y < dst_height; y++)
    {
        yn = 1.0f * y / dst_height;
        fys = yn * src_height;
        iys0 = fys;
        iys1 = iys0 + 1;
        fys -= iys0;
        if(iys1 >= src_height) iys1 = src_height - 1;
        for(x = 0; x < dst_width; x++)
        {
            xn = 1.0f * x / dst_width;
            fxs = xn * src_width;
            ixs0 = fxs;
            ixs1 = ixs0 + 1;
            if(ixs1 >= src_width) ixs1 = src_width - 1;
            fxs -= ixs0;

            sample_left = src[iys0 * src_width + ixs0];
            sample_right = src[iys0 * src_width + ixs1];
            upper_middle = cosine_interpolationf(sample_left, sample_right, fxs);

            sample_left = src[iys1 * src_width + ixs0];
            sample_right = src[iys1 * src_width + ixs1];
            lower_middle = cosine_interpolationf(sample_left, sample_right, fxs);

            result = cosine_interpolationf(upper_middle, lower_middle, fys);

            dst[y * dst_width + x] = result;


        }
    }


}

void float_map_multiply_and_add(float *dst, float *src, int w, int h, float multiplier)
{
    int x, y;
    for(y = 0; y < h; y++)
    {
        for(x = 0; x < w; x++)
        {
            dst[y * w + x] += src[y * w + x] * multiplier;
        }
    }
}


rafgl_raster_t generate_perlin_noise(int octaves, float persistance)
{
    int octave_size = 2;
    float multiplier = 1.0f;

    rafgl_raster_t r;

    int width = pow(2, octaves);
    int height = width;

    int octave, x, y;
    float *tmp_map = malloc(height * width * sizeof(float));
    float *final_map = calloc(height * width, sizeof(float));
    float *octave_map;
    rafgl_pixel_rgb_t pix;
    rafgl_raster_init(&r, width, height);


    for(octave = 0; octave < octaves; octave++)
    {
        octave_map = malloc(octave_size * octave_size * sizeof(float));
        for(y = 0; y < octave_size; y++)
        {
            for(x = 0; x < octave_size; x++)
            {
                octave_map[y * octave_size + x] = randf() * 2.0f - 1.0f;
            }
        }

        cosine_float_map_rescale(tmp_map, width, height, octave_map, octave_size, octave_size);
        float_map_multiply_and_add(final_map, tmp_map, width, height, multiplier);

        octave_size *= 2;
        multiplier *= persistance;

        free(octave_map);
    }

    float sample;
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            sample = final_map[y * width + x];
            sample = (sample + 1.0f) / 2.0f;
            if(sample < 0.0f) sample = 0.0f;
            if(sample > 1.0f) sample = 1.0f;
            pix.r = sample * 255;
            pix.g = sample * 255;
            pix.b = sample * 255;

            pixel_at_m(r, x, y) = pix;
        }
    }


    free(final_map);
    free(tmp_map);

    return r;

}

    /** stars */
typedef struct _star_t
{
    int x, y, z;
} star_t;


#define STAR_MAX 500
#define STAR_Z_MAX 2000
star_t stars[STAR_MAX];


void init_stars(void)
{
    int i;
    for(i = 0; i < STAR_MAX; i++)
    {

        stars[i].z = randf() * STAR_Z_MAX;
        stars[i].x = randf() * STAR_Z_MAX - STAR_Z_MAX / 2;
        stars[i].y = randf() * STAR_Z_MAX - STAR_Z_MAX / 2;
    }
}

void update_stars(int speed)
{
    svemir_life++;
    int i;
    for(i = 0; i < STAR_MAX; i++)
    {
        stars[i].z -= speed;
        if(stars[i].z <= 1)
        {
            stars[i].z += STAR_Z_MAX;
            stars[i].x = randf() * STAR_Z_MAX - STAR_Z_MAX / 2;
            stars[i].y = randf() * STAR_Z_MAX - STAR_Z_MAX / 2;
        }
    }
}

void render_stars(rafgl_raster_t *raster, int speed)
{
    int i, brightness;
    float sx0, sy0, sx1, sy1;

    for(i = 0; i < STAR_MAX; i++)
    {
        sx0 = raster_width / 2 + stars[i].x * (0.5 * raster_width / stars[i].z);
        sy0 = raster_height / 2 + stars[i].y * (0.5 * raster_height / stars[i].z);

        sx1 = raster_width / 2 + stars[i].x * (0.5 * raster_width / (stars[i].z + speed));
        sy1 = raster_height / 2 + stars[i].y * (0.5 * raster_height / (stars[i].z + speed));

        brightness = 255 - ((float)stars[i].z / STAR_Z_MAX) * 255.0f;

        rafgl_raster_draw_line(raster, sx0, sy0, sx1, sy1, rafgl_RGB(brightness, brightness, brightness));

    }

}

void shutdown_gameover(int flag){
    int x, y;
    radius-=20;
    if(radius>=-30){ // shut down effect

        float distance;

        for(y = 0; y < raster_height; y++)
        {

            for(x = 0; x < raster_width; x++)
            {
                // da bi centar kruga bio u centru slicice
                distance = rafgl_distance2D(RASTER_WIDTH/2+fire.frame_width/2, RASTER_HEIGHT/2+fire.frame_height/2, x, y);

                if(distance > radius) {
                    // da u krugu ostane mapa
                    pixel_at_m(raster, x, y) = background_colour;
                }
            }
        }
    } else if(radius>=-260){
        // delay
    } else {
        // gotov shut down effect
        // game over iscrtavanje

        int perlin_brightness; //delta_brightness=1 na pocetku
        for(y=0; y<raster_height; y++){
            for(x=0; x<raster_width; x++){
                if(flag==1){
                    perlinpix = pixel_at_m(perlin,x,y);
                    gameoverpix = pixel_at_m(gameover,x,y);
                    perlin_brightness = (perlinpix.r + perlinpix.g + perlinpix.b)/3;
                    if(perlin_brightness < delta_brightness){ // prvo crta najtamnije delove perlina
                        pixel_at_m(raster,x,y) = gameoverpix;
                    }
                } else if (flag==2){
                    for(x=0; x<raster_width; x++){
                        int i=rand()%RASTER_WIDTH,j=rand()%RASTER_HEIGHT;
                        gameoverpix2 = pixel_at_m(gameover2,i,j);
                        pixel_at_m(raster,i,j) = gameoverpix2;
                    }
                }
            }
        }
        delta_brightness++;
    }
}

void drunk_blur(){
    int x, y, xs, ys, offset;

    int radius = 5;
    int sample_count = 2 * radius + 1;

    int r, g, b;

    rafgl_pixel_rgb_t sampled, resulting;

    for(y = 0; y < RASTER_HEIGHT; y++)
    {
        for(x = 0; x < RASTER_WIDTH; x++)
        {
            r = g = b = 0;
            for(offset = -radius; offset <= radius; offset++)
            {
                xs = rafgl_clampi(x + offset, 0, RASTER_WIDTH);
                ys = y;
                sampled = pixel_at_m(raster, xs, ys);
                r += sampled.r;
                g += sampled.g;
                b += sampled.b;

            }

            resulting.r = r / sample_count;
            resulting.g = g / sample_count;
            resulting.b = b / sample_count;

            pixel_at_m(tmp, x, y) = resulting;
        }
    }

    for(y = 0; y < RASTER_HEIGHT; y++)
    {
        for(x = 0; x < RASTER_WIDTH; x++)
        {
            r = g = b = 0;
            for(offset = -radius; offset <= radius; offset++)
            {
                xs = x;
                ys = rafgl_clampi(y + offset, 0, RASTER_HEIGHT);
                sampled = pixel_at_m(tmp, xs, ys);
                r += sampled.r;
                g += sampled.g;
                b += sampled.b;

            }

            resulting.r = r / sample_count;
            resulting.g = g / sample_count;
            resulting.b = b / sample_count;

            pixel_at_m(raster, x, y) = resulting;
        }
    }
}

void fire_effect(){
    fire_life++;
    if(fire_life<75){
        // fire effect + sirenje grayscale kruga
        rafgl_raster_draw_spritesheet(&raster, &fire, animation_frame_fire,direction_fire, hero_pos_x, hero_pos_y);
        grayscale_radius+=20;
        int x, y;
        float distance;

        for(y = 0; y < RASTER_HEIGHT; y++)
        {
            for(x = 0; x < RASTER_WIDTH; x++)
            {
                distance = rafgl_distance2D(RASTER_WIDTH/2+hero.frame_width/2, RASTER_HEIGHT/2+hero.frame_height/2, x, y);

                if(distance > grayscale_radius) {
                    // u krugu ostane mapa prave boje
                    // van kruga neistrazeno
                    sampled = pixel_at_m(raster, x, y);

                    int brightness;
                    brightness = (sampled.r + sampled.g + sampled.b) / 3;

                    resulting.r = brightness;
                    resulting.g = brightness;
                    resulting.b = brightness;

                    pixel_at_m(raster, x, y) = resulting;
                }
            }
        }
    } else if(fire_life<90){
        // delay
    } else {
        shutdown_gameover(1);
    }
}

void space_effect(){
    float xn, yn;
    if(svemir_life<100){
        int star_speed = 40;

        update_stars(star_speed);
        /* izmeni raster */

        int x, y;

        rafgl_pixel_rgb_t sampled, resulting;


        for(y = top; y < bottom; y++)
        {
            yn = 1.0f * y / (bottom-top);
            for(x = left; x < right; x++)
            {
                xn = 1.0f * x / (right-left);

                sampled = pixel_at_m(raster, x, y);

                resulting = sampled;
                resulting.rgba = rafgl_RGB(0, 0, 0);
                pixel_at_m(raster, x, y) = resulting;
            }
        }

        render_stars(&raster, star_speed);

        // efekat sirenja kvadrata
        left-=20;right+=20;top-=20;bottom+=20;
        // da ne bi crashovalo jer izadje iz opsega
        if(left<0) left=0;
        if(right>RASTER_WIDTH) right = RASTER_WIDTH;
        if(top<0) top=0;
        if(bottom>RASTER_HEIGHT) bottom = RASTER_HEIGHT;

        rafgl_raster_draw_spritesheet(&raster, &hero, animation_frame, direction, hero_pos_x, hero_pos_y);
    } else {
        shutdown_gameover(2);
    }
}

void shadow_effect(){
    rafgl_raster_draw_spritesheet(&raster, &hero, animation_frame, direction, hero_pos_x, hero_pos_y);

    // hero u vidokrugu + grayscale krug
    int x, y;
    float distance;

    for(y = 0; y < RASTER_HEIGHT; y++)
    {
        for(x = 0; x < RASTER_WIDTH; x++)
        {
            distance = rafgl_distance2D(RASTER_WIDTH/2+hero.frame_width/2, RASTER_HEIGHT/2+hero.frame_height/2, x, y);

            if(distance > grayscale_radius) {
                // u krugu ostane mapa prave boje
                // van kruga neistrazeno
                sampled = pixel_at_m(raster, x, y);

                int brightness;
                brightness = (sampled.r + sampled.g + sampled.b) / 3;

                resulting.r = brightness;
                resulting.g = brightness;
                resulting.b = brightness;

                pixel_at_m(raster, x, y) = resulting;
            }
        }
    }
}

void start_noise_effect(){
    int x, y, pom;
    for(y = 0; y < raster_height; y++)
    {
        for(x = 0; x < raster_width; x++)
        {
            if(x<200 || x>600 || y<300 || y>500){
                pom = rand() % 255;
                pixel_at_m(raster, x, y).rgba = rafgl_RGB(pom, pom, pom);
            } else {
                pixel_at_m(raster, x, y) = pixel_at_m(startraster,x-200,y-300);
            }
        }
    }

    rafgl_texture_load_from_raster(&texture, &raster);
}

void draw_background(){
    int x,y;
    float xn,yn;
    // drawing background
    for(y = 0; y < raster_height; y++)
    {
        yn = 1.0f * y / raster_height;
        for(x = 0; x < raster_width; x++)
        {
            xn = 1.0f * x / raster_width;

            sampled = pixel_at_m(upscaled_doge, x, y);
            sampled2 = rafgl_point_sample(&doge, xn, yn);

            resulting = sampled;
            resulting2 = sampled2;

            resulting.rgba = rafgl_RGB(0, 0, 0);
            resulting = sampled; //background

            pixel_at_m(raster, x, y) = resulting;
            pixel_at_m(raster2, x, y) = resulting2;
        }
    }
}

bool is_out_of_world(int x,int y){
    if(x<0 || x>127 || y<-1 || y>127)
        return true;
    else
        return false;
}

void win_effect(){
    int x, y;
    float distance;

    for(y = 0; y < RASTER_HEIGHT; y++)
    {
        pixel_at_m(raster,win_x, y) = pixel_at_m(winraster,win_x, y);
    }
    win_x++;
    if(win_x>RASTER_WIDTH)
        win_x=RASTER_WIDTH;
}

    /** MAIN STATE INIT */
void main_state_init(GLFWwindow *window, void *args)
{
    /* inicijalizacija */
    /* raster init nam nije potreban ako radimo load from image */
    rafgl_raster_load_from_image(&doge, "res/images/fire_backgroundd.png");
    rafgl_raster_load_from_image(&checker, "res/images/checker32.png");
    rafgl_raster_load_from_image(&gameover, "res/images/game_over.png");
    rafgl_raster_load_from_image(&gameover2, "res/images/game_over_space.png");
    rafgl_raster_load_from_image(&startraster, "res/images/start.png");
    rafgl_raster_load_from_image(&winraster, "res/images/win.png");


    rafgl_raster_init(&upscaled_doge, raster_width, raster_height);
    rafgl_raster_bilinear_upsample(&upscaled_doge, &doge);

    rafgl_raster_init(&raster, raster_width, raster_height);
    rafgl_raster_init(&raster2, raster_width, raster_height);
    rafgl_raster_init(&tmp, RASTER_WIDTH, RASTER_HEIGHT);
    rafgl_spritesheet_init(&hero, "res/images/character.png", 10, 4);
    rafgl_spritesheet_init(&fire, "res/images/fire.png", 4, 3);

    perlin = generate_perlin_noise(10, 0.7f);
    init_stars();

    int i;

    char tile_path[256];

    for(i = 0; i < NUMBER_OF_TILES; i++)
    {
        sprintf(tile_path, "res/tiles/svgset%d.png", i);
        rafgl_raster_load_from_image(&tiles[i], tile_path);
    }


    init_tilemap();
    rafgl_texture_init(&texture);

    background_colour.rgba = rafgl_RGB(0, 0, 0);
}


    /** MAIN STATE UPDATE */
void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args)
{
    if(game_data->keys_pressed[RAFGL_KEY_ENTER]){ // press enter for start
        start=1;
    }

    if(start==0){
        start_noise_effect(); // grayscale noise
    } else {

        selected_x = rafgl_clampi((game_data->mouse_pos_x + camx) / TILE_SIZE, 0, WORLD_SIZE - 1);
        selected_y = rafgl_clampi((game_data->mouse_pos_y + camy) / TILE_SIZE, 0, WORLD_SIZE - 1);
        int  s_x = (hero_pos_x + 32 + camx) / TILE_SIZE; // position of hero in the world
        int s_y = (hero_pos_y + 32 + camy) / TILE_SIZE; // 32 = hero size / 2

        if(tile_world[s_y][s_x]==6){
            drunk=1;
        }else{
            drunk=0;
        }
        if(tile_world[s_y][s_x]==7){
            svemir=1;
        } else {
            svemir=0;
        }
        if (tile_world[s_y][s_x]==19){
            win=1;
        } else {
            win=0;
        }

        animation_running = 1;

        if(game_data->keys_down[RAFGL_KEY_W])
        {
            //da kad se prebaci na fire spritesheet ne mozes nigde nego goris i umires
            if(fire_life==0 && svemir_life==0 && win==0){
                camy-=10;
                direction = 2;
            }
        }
        else if(game_data->keys_down[RAFGL_KEY_S])
        {
            if(fire_life==0 && svemir_life==0 && win==0){
                camy+=10;
                direction = 0;
            }
        }
        else if(game_data->keys_down[RAFGL_KEY_A])
        {
            if(fire_life==0 && svemir_life==0 && win==0){
               camx-=10;
                direction = 1;
            }
        }
        else if(game_data->keys_down[RAFGL_KEY_D])
        {
            if(fire_life==0 && svemir_life==0 && win==0){
                camx+=10;
                direction = 3;
            }
        }
        else
        {
            animation_running = 0;
        }

        if(animation_running)
        {
            if(hover_frames == 0)
            {
                animation_frame = (animation_frame + 1) % 10;
                hover_frames = 2;
            }
            else
            {
                hover_frames--;
            }
        }

        if(hover_fire_frames==0){
            animation_frame_fire = (animation_frame_fire+1) %4;
            direction_fire=(direction_fire+1)%3;
            hover_fire_frames=8;
        } else {
            hover_fire_frames--;
        }

        int x, y;

        float xn, yn;

        if(fire_life<110 && svemir_life<100 && win_x==0){ // da bi game over / win iscrtavanje bilo kako treba
            draw_background();
            if(game_data->keys_pressed[RAFGL_KEY_0])
            {
                tile_world[selected_y][selected_x]++;
                tile_world[selected_y][selected_x] %= NUMBER_OF_TILES;
            }
            // drawing tiles on top of background
            render_tilemap(&raster);
        }

        // drawing spritesheet on top of tiles
        if(is_out_of_world(s_x,s_y)){ // -1 jer je spritesheet vatre malo duzi pa da ne upada u world
            fire_effect();
        } else {
            //hero lost in space
            if(svemir){
                space_effect();
            } else {
                if(win_x==0) // da mi ne bi grayscaleovao win sliku
                    shadow_effect();
            }
        }

        //drunk = box blur
        if(drunk){
            drunk_blur();
        }
        //win
        if(win){
            win_effect();
        }

        // update-uj teksturu
        if(!game_data->keys_down[RAFGL_KEY_SPACE])
            rafgl_texture_load_from_raster(&texture, &raster);
        else
            rafgl_texture_load_from_raster(&texture, &perlin);

    }
}


void main_state_render(GLFWwindow *window, void *args)
{
    rafgl_texture_show(&texture);
}


void main_state_cleanup(GLFWwindow *window, void *args)
{
    rafgl_raster_cleanup(&raster);
    rafgl_raster_cleanup(&raster2);
    rafgl_raster_cleanup(&perlin);
    rafgl_raster_cleanup(&gameover);
    rafgl_raster_cleanup(&gameover2);
    rafgl_raster_cleanup(&startraster);
    rafgl_raster_cleanup(&winraster);
    rafgl_texture_cleanup(&texture);

}
