#include "videooutsdl.h"

namespace LQF
{
    static bool bSDL_INIT_VIDEO = false;
    VideoOutSDL::VideoOutSDL()
    {
    }

    VideoOutSDL::~VideoOutSDL()
    {
        if (texture_)
            SDL_DestroyTexture(texture_);
        if (renderer_)
            SDL_DestroyRenderer(renderer_);
        if (win_)
            SDL_DestroyWindow(win_);

        SDL_Quit();
    }

    RET_CODE VideoOutSDL::Init(const Properties &properties)
    {
        video_width_ = properties.GetProperty("video_width", -1);
        if (-1 == video_width_)
        {
            LogError("video_width no set");
            return RET_FAIL;
        }
        video_height_ = properties.GetProperty("video_height", -1);
        if (-1 == video_height_)
        {
            LogError("video_height no set");
            return RET_FAIL;
        }

        pixformat_ = properties.GetProperty("pixformat", SDL_PIXELFORMAT_IYUV);
        win_x_ = properties.GetProperty("win_x", SDL_WINDOWPOS_UNDEFINED);
        win_y_ = properties.GetProperty("win_y", SDL_WINDOWPOS_UNDEFINED);
        win_width_ = properties.GetProperty("win_width", video_width_);
        win_height_ = properties.GetProperty("win_height", video_height_);
        win_title_ = properties.GetProperty("win_title", "sdl display");
        //初始化 SDL
        if (SDL_Init(SDL_INIT_VIDEO))
        {
            LogError("Could not initialize SDL - %s", SDL_GetError());
            return RET_FAIL;
        }
        //创建窗口
        win_ = SDL_CreateWindow(win_title_.c_str(),
                                win_x_,
                                win_y_,
                                win_width_, win_height_,
                                SDL_WINDOW_RESIZABLE);
        if (!win_)
        {
            LogError("SDL: could not create window, err:%s", SDL_GetError());
            return RET_FAIL;
        }

        // 基于窗口创建渲染器
        renderer_ = SDL_CreateRenderer(win_, -1, 0);
        if (!renderer_)
        {
            LogError("SDL: could not create renderer_, err:%s", SDL_GetError());
            return RET_FAIL;
        }
        // 基于渲染器创建纹理
        texture_ = SDL_CreateTexture(renderer_,
                                     pixformat_,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     video_width_,
                                     video_height_);
        if (!texture_)
        {
            LogError("SDL: could not create texture_, err:%s", SDL_GetError());
            return RET_FAIL;
        }
        LogInfo("%s init done", win_title_.c_str());
        return RET_OK;
    }

    RET_CODE VideoOutSDL::Output(const uint8_t *video_buf, const uint32_t size)
    {
        // 设置纹理的数据
        SDL_UpdateTexture(texture_, NULL, video_buf, video_width_);

        //FIX: If window is resize
        rect_.x = 0;
        rect_.y = 0;
        rect_.w = video_width_;
        rect_.h = video_height_;

        // 清除当前显示
        if (SDL_RenderClear(renderer_) != 0)
        {
            LogError("SDL_RenderClear failed");
            return RET_FAIL;
        }
        // 将纹理的数据拷贝给渲染器
        if (SDL_RenderCopy(renderer_, texture_, NULL, &rect_) != 0)
        {
            LogError("SDL_RenderCopy failed");
            return RET_FAIL;
        }
        // 显示
        SDL_RenderPresent(renderer_);
        return RET_OK;
    }

} // namespace LQF
