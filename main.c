#include "helper.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

int phy_iter = 4;

#define MAX_BALLS 1024
int w_height            = 720;
int w_width             = 1080;
float ball_rad          = 30;
float pw                = 10;
float ph                = 180;
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

float get_final_pos(float start, float speed, float time, float range, float height)
{
    float final = start + speed * time;

    if (speed > 0)
    {
        int bounces  = (int)(final - height * 0.5) / (int)range;
        float left_y = fmodf(final - height * 0.5, range);
        if (bounces % 2 != 0)
        {
            final = w_height - (height * 0.5) - left_y;
        }
        else
        {
            final = left_y + (height * 0.5);
        }
    }
    else
    {
        float dist = (w_height - height * 0.5f) - final;

        int bounces  = (int)dist / (int)range;
        float left_y = fmodf(dist, range);

        if (bounces % 2 == 0)
        {
            final = w_height - (height * 0.5) - left_y;
        }
        else
        {
            final = left_y + (height * 0.5);
        }
    }
    return final;
}

void generate_path(int idx, int paddle_i)
{
    // the target paddle is opposite to the one we're provided
    const struct Paddle paddle = paddles[paddle_i ^ 1];
    struct Ball *ball          = &balls[idx];

    if (!ball) return;

    const float brh = ball_rad * 0.5;

    if (paddle.x > ball->x)
    {
        ball->dir_x = fabsf(ball->dir_x);
    }
    else
    {
        ball->dir_x = -fabsf(ball->dir_x);
    }

    const float vx = ball->dir_x * ball->speed;
    const float vy = ball->dir_y * ball->speed;

    const float dist = fabsf(ball->x - paddle.x) - (pw * 0.5 + brh);
    const float time = dist / fabsf(vx);

    const float p_range = w_height - ph;
    const float p_y     = get_final_pos(paddle.y, paddle.speed, time, p_range, ph);

    const float b_range = w_height - ball_rad;
    const float b_y     = ball->y + vy * time;

    int bounces = (int)floorf((b_y - brh) / b_range);

    // calculate 'n', where n is the number of bounces for which the final pos of ball is nearest to the paddle. n = [bounces - 2, bounces + 2]
    float target = 0.0f;
    float min    = 67676767.0f;
    for (int i = bounces - 2; i <= bounces + 2; i++)
    {
        float y = 0.0f;

        if (i % 2 == 0)
        {
            y = (i * b_range) + p_y;
        }
        else
        {
            y = brh + (i * b_range) + (b_range - (p_y - brh));
        }

        if (fabsf(y - b_y) < min)
        {
            target = y;
            min    = fabsf(y - b_y);
        }
    }

    const float dy  = (target - ball->y);
    const float nvy = dy / time;

    ball->speed = sqrtf(vx * vx + nvy * nvy);
    ball->dir_x = vx / ball->speed;
    ball->dir_y = nvy / ball->speed;
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

        const float d_x = balls[idx].dir_x;
        const float d_y = balls[idx].dir_y;

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
        generate_path(idx, i);

        // spawn another ball
        if (current_balls >= MAX_BALLS) return;

        float x     = balls[idx].x;
        float y     = balls[idx].y;
        float dir_x = -d_x;
        float dir_y = -d_y;
        float speed = (float)(200 + (rand() % (300 - 200)));
        add_ball(x, y, dir_x, dir_y, speed);

        int new_idx            = current_balls - 1;
        balls[new_idx].iframes = 0.5f;
        generate_path(new_idx, i);

        return;
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
    float py  = paddles[index].y - (ph * 0.5f);
    float pym = paddles[index].y + (ph * 0.5f);

    if (py < 0)
    {
        paddles[index].speed = fabsf(paddles[index].speed);
        paddles[index].y     = (ph * 0.5f) - py;
    }
    else if (pym > w_height)
    {
        paddles[index].speed = -fabsf(paddles[index].speed);
        paddles[index].y     = (w_height - ph * 0.5f) - (pym - w_height);
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
    for (int i = 0; i < current_balls; i++)
    {
        generate_path(i, rand() % 2);
    }
}

Uint64 last_tick = 0;
float update()
{
    Uint64 ct  = SDL_GetTicksNS();
    float dt   = (float)(ct - last_tick) / 1000000000.0;
    dt        /= phy_iter;
    last_tick  = ct;

    for (int kew = 0; kew < phy_iter; kew++)
    {
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
    else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_SPACE)
    {
        static bool first = true;
        if (!first) return SDL_APP_CONTINUE;

        first = false;

        float x     = (float)(300 + (rand() % (500 - 300)));
        float y     = (float)(300 + (rand() % (500 - 300)));
        float dir_x = (float)(rand() % 2000 - 1000) / 1000.0f;
        float dir_y = (float)(rand() % 2000 - 1000) / 1000.0f;
        float speed = (float)(200 + (rand() % (300 - 200)));
        add_ball(x, y, dir_x, dir_y, speed);
        generate_path(0, rand() % 2);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColorFloat(renderer, 0.1, 0.2, 0.3, 1.0);
    SDL_RenderClear(renderer);

    Uint64 t0 = SDL_GetTicksNS();
    float dt  = update() * (float)phy_iter;

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
