#include "helper.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define MAX_BALLS 1024
int w_height            = 720;
int w_width             = 1080;
float ball_rad          = 30;
float pw                = 10;
float ph                = 100;
SDL_FColor paddle_color = {
    .r = 1.0,
    .g = 1.0,
    .b = 1.0,
    .a = 1.0,
};
SDL_Window *window          = NULL;
SDL_Renderer *renderer      = NULL;
SDL_Texture *circle_texture = NULL;

struct Ball
{
    float x;
    float y;
    float dir_x;
    float dir_y;
    float speed;
    float iframes;
    SDL_Color color;
};

struct Ball balls[MAX_BALLS];
int current_balls = 0;

struct Paddle
{
    float x;
    float y;
    float speed;
};

struct Paddle paddles[2];

/// circle_texture width&height
float tw = 50.0;

bool init_textures()
{
    circle_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, tw, tw);
    SDL_SetTextureBlendMode(circle_texture, SDL_BLENDMODE_BLEND);

    SDL_SetRenderTarget(renderer, circle_texture);
    SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 0.0f);
    SDL_RenderClear(renderer);

    float cx = tw / 2.0;

    for (int i = 0; i < tw; i++)
    {
        for (int j = 0; j < tw; j++)
        {
            float dx  = (float)i - cx;
            float dy  = (float)j - cx;
            dx       *= dx;
            dy       *= dy;

            float norm_dist = sqrt(dx + dy) / cx;
            float m         = 1.0 - smoothstep(0.9, 1.0, norm_dist);

            SDL_SetRenderDrawColorFloat(renderer, 1.0, 1.0, 1.0, m);

            SDL_RenderPoint(renderer, i, j);
        }
    }

    SDL_SetRenderTarget(renderer, NULL);
    return true;
}

SDL_Color hsv_converter(float h, float s, float v)
{
    SDL_Color color = {};

    int h_i = (int)(h * 6);
    float f = h * 6 - h_i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (h_i)
    {
        case 0:
            color.r = (int)(256 * v);
            color.g = (int)(256 * t);
            color.b = (int)(256 * p);
            break;
        case 1:
            color.r = (int)(256 * q);
            color.g = (int)(256 * v);
            color.b = (int)(256 * p);
            break;
        case 2:
            color.r = (int)(256 * p);
            color.g = (int)(256 * v);
            color.b = (int)(256 * t);
            break;
        case 3:
            color.r = (int)(256 * p);
            color.g = (int)(256 * q);
            color.b = (int)(256 * v);
            break;
        case 4:
            color.r = (int)(256 * t);
            color.g = (int)(256 * p);
            color.b = (int)(256 * v);
            break;
        case 5:
            color.r = (int)(256 * v);
            color.g = (int)(256 * p);
            color.b = (int)(256 * q);
            break;
        default:
            break;
    };

    return color;
}

/// https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
SDL_Color generate_color()
{
    const float inv_golden_ratio = 0.618033988749895;
    static float h               = 0;
    static bool first            = true;

    if (first)
    {
        first = false;
        h     = (float)(rand() % 1000) / 1000.0f;
    }
    else
    {
        h += inv_golden_ratio;
        h -= (int)h;  // conversion magic
    }

    return hsv_converter(h, 0.5, 0.95);
}

void add_ball(float x, float y, float dir_x, float dir_y, float speed)
{
    if (current_balls == MAX_BALLS) return;

    // normalise direction
    float mag  = sqrtf(dir_x * dir_x + dir_y * dir_y);
    dir_x     /= mag;
    dir_y     /= mag;

    balls[current_balls++] = (struct Ball) {
        .x       = x,
        .y       = y,
        .dir_x   = dir_x,
        .dir_y   = dir_y,
        .speed   = speed,
        .iframes = 0,
        .color   = generate_color(),
    };
}

void do_ball_collision(int idx)
{
    if (balls[idx].iframes > 0) return;

    float brh = ball_rad * 0.5;
    for (int i = 0; i < 2; i++)
    {
        float dx = fabsf(paddles[i].x - balls[idx].x);
        float dy = fabsf(paddles[i].y - balls[idx].y);

        float min_x = (pw * 0.5f) + brh;
        float min_y = (ph * 0.5f) + brh;

        if (dx > min_x) { continue; }
        if (dy > min_y) { continue; }

        // collision confirmed
        balls[idx].iframes = 0.5;

        // overlap
        float ox = dx - min_x;
        float oy = dy - min_y;

        if (ox > oy)
        {
            if (balls[idx].x < paddles[i].x)
            {
                balls[idx].x     -= ox;
                balls[idx].dir_x  = -fabsf(balls[idx].dir_x);
            }
            else
            {
                balls[idx].x     += ox;
                balls[idx].dir_x  = fabsf(balls[idx].dir_x);
            }
        }
        else
        {
            if (balls[idx].y < paddles[i].y)
            {
                balls[idx].y     -= oy;
                balls[idx].dir_y  = -fabsf(balls[idx].dir_y);
            }
            else
            {
                balls[idx].y     += oy;
                balls[idx].dir_y  = fabsf(balls[idx].dir_y);
            }
        }
    }
}

void update_ball(int idx)
{
    float brh = ball_rad * 0.5;

    // check collision with both paddles
    do_ball_collision(idx);

    // out of bounds, bounce in
    if (balls[idx].x - brh < 0)
    {
        balls[idx].x     = brh;
        balls[idx].dir_x = fabsf(balls[idx].dir_x);
    }
    else if (balls[idx].x + brh > w_width)
    {
        balls[idx].x     = w_width - brh;
        balls[idx].dir_x = -fabsf(balls[idx].dir_x);
    }

    if (balls[idx].y - brh < 0)
    {
        balls[idx].y     = brh;
        balls[idx].dir_y = fabsf(balls[idx].dir_y);
    }
    else if (balls[idx].y + brh > w_height)
    {
        balls[idx].y     = w_height - brh;
        balls[idx].dir_y = -fabsf(balls[idx].dir_y);
    }
}

void paddle_flip(int index)
{
    float py  = paddles[index].y - (ph * 0.5);
    float pym = paddles[index].y + (ph * 0.5);

    if (py < 0 || pym > w_height)
    {
        paddles[index].speed *= -1;
        paddles[index].y      = (paddles[index].speed > 0.0) ? ph * 0.5 : w_height - ph * 0.5;
    }
}

void set_initials(int width, int height)
{
    float min_dist = 200.0;
    float a        = 0.8;

    float xs[2] = {0, 0};

    xs[0] = (width / 2.0) - width * a * 0.5;
    xs[1] = (width / 2.0) + width * a * 0.5;

    if (width * a <= min_dist)
    {
        xs[0] = (width / 2.0) - min_dist * 0.5;
        xs[1] = (width / 2.0) + min_dist * 0.5;
    }

    int max_speed = 400;
    int min_speed = 350;

    static bool first_time = true;

    for (int i = 0; i < 2; i++)
    {
        paddles[i].x = xs[i];

        if (first_time)
        {
            int py           = w_height - ph;
            paddles[i].y     = (rand() % py) + ph * 0.5;
            paddles[i].speed = min_speed + rand() % (max_speed - min_speed);
        }
    }
    first_time = false;
}

void window_resized()
{
    SDL_GetWindowSize(window, &w_width, &w_height);
    set_initials(w_width, w_height);
}

Uint64 last_tick = 0;
float update()
{
    Uint64 ct = SDL_GetTicksNS();
    float dt  = (float)(ct - last_tick) / 1000000000.0;
    last_tick = ct;

    for (int i = 0; i < 2; i++)
    {
        paddles[i].y += paddles[i].speed * dt;
        paddle_flip(i);
    }

    for (int i = 0; i < current_balls; i++)
    {
        balls[i].x += balls[i].dir_x * balls[i].speed * dt;
        balls[i].y += balls[i].dir_y * balls[i].speed * dt;

        balls[i].iframes -= dt;
        update_ball(i);
    }

    return dt;
}

void render()
{
    float w, h;
    SDL_GetTextureSize(circle_texture, &w, &h);

    float brh = ball_rad * 0.5;
    for (int i = 0; i < current_balls; i++)
    {
        SDL_SetTextureColorMod(circle_texture, balls[i].color.r, balls[i].color.g, balls[i].color.b);
        SDL_FRect src = {
            .x = 0,
            .y = 0,
            .w = w,
            .h = h,
        };

        SDL_FRect dst = {
            .x = balls[i].x - brh,
            .y = balls[i].y - brh,
            .w = ball_rad,
            .h = ball_rad,
        };

        SDL_RenderTexture(renderer, circle_texture, &src, &dst);
    }

    for (int i = 0; i < 2; i++)
    {
        SDL_FRect rect = {
            .x = paddles[i].x - pw * 0.5,
            .y = paddles[i].y - ph * 0.5,
            .w = pw,
            .h = ph,
        };
        SDL_SetRenderDrawColorFloat(renderer, paddle_color.r, paddle_color.g, paddle_color.b, paddle_color.a);
        SDL_RenderFillRect(renderer, &rect);
    }
}

bool blog(bool result)
{
    if (!result)
    {
        SDL_Log("SDL_Error: %s", SDL_GetError());
    }
    return result;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    srand(time(NULL));

    SDL_SetAppMetadata("Zero Player Pong", "1.0", "cc.aroy.game");

    if (!blog(SDL_Init(SDL_INIT_VIDEO)))
    {
        return SDL_APP_FAILURE;
    }

    if (!blog(SDL_CreateWindowAndRenderer("Zero Player Pong", w_width, w_height, SDL_WINDOW_RESIZABLE, &window, &renderer)))
    {
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderVSync(renderer, 1);

    if (!init_textures())
    {
        return SDL_APP_FAILURE;
    };

    set_initials(w_width, w_height);

    for (int i = 0; i < MAX_BALLS; i++)
    {
        float x     = (float)(300 + (rand() % (500 - 300)));
        float y     = (float)(300 + (rand() % (500 - 300)));
        float dir_x = (float)(rand() % 2000 - 1000) / 1000.0f;
        float dir_y = (float)(rand() % 2000 - 1000) / 1000.0f;
        float speed = (float)(200 + (rand() % (300 - 200)));
        add_ball(x, y, dir_x, dir_y, speed);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    else if (event->window.type == SDL_EVENT_WINDOW_RESIZED)
    {
        window_resized();
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColorFloat(renderer, 0.1, 0.2, 0.3, 1.0);
    SDL_RenderClear(renderer);

    Uint64 t0 = SDL_GetTicksNS();
    float dt  = update();

    Uint64 t1 = SDL_GetTicksNS();
    render();

    Uint64 t2 = SDL_GetTicksNS();
    SDL_RenderPresent(renderer);

    Uint64 t3 = SDL_GetTicksNS();

    float update_ms  = (float)(t1 - t0) / 1000000.0f;
    float render_ms  = (float)(t2 - t1) / 1000000.0f;
    float present_ms = (float)(t3 - t2) / 1000000.0f;

    printf("\rframe time: %.3fms | update: %.3fms | render: %.3fms | wait: %.3fms",
           dt * 1000.0f,
           update_ms,
           render_ms,
           present_ms);
    fflush(stdout);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    printf("\nBye!\n");
}
