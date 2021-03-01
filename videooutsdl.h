#ifndef VIDEOOUTSDL_H
#define VIDEOOUTSDL_H

#include <SDL2/SDL.h>
#include "mediabase.h"

namespace LQF
{
    class VideoOutSDL
    {
    public:
        VideoOutSDL();
        virtual ~VideoOutSDL();

        /**
     * @brief Init 初始化显示参数
     * @param "video_width" 画面宽度,必须设置
     *        "video_height" 画面高度,必须设置
     *        "pixformat"像素格式，缺省为SDL_PIXELFORMAT_IYUV
     *        "win_x_", 窗口的起始位置, 缺省位于屏幕中间显示
     *        "win_y_", 窗口的起始位置, 缺省位于屏幕中间显示
     *        "win_width" 窗口宽度,缺省为video_width
     *        "win_height" 窗口高度,video_height
     *        "win_title" 窗口名字, 缺省为"sdl display"
     *
     * @return
     */
        virtual RET_CODE Init(const Properties &properties);

        virtual RET_CODE Output(const uint8_t *video_buf, const uint32_t size);

    private:
        SDL_Event event_;               // 事件
        SDL_Rect rect_;                 // 矩形
        SDL_Window *win_ = NULL;        // 窗口
        SDL_Renderer *renderer_ = NULL; // 渲染
        SDL_Texture *texture_ = NULL;   // 纹理

        // Init涉及的参数设置
        int video_height_ = 720;
        int video_width_ = 1280;
        int pixformat_ = SDL_PIXELFORMAT_IYUV; // YUV420P, 即SDL_PIXELFORMAT_IYUV
        int win_x_ = SDL_WINDOWPOS_UNDEFINED;
        int win_y_ = SDL_WINDOWPOS_UNDEFINED;
        int win_width_ = 1280;
        int win_height_ = 720;
        std::string win_title_;
    };
}

#endif // VIDEOOUTSDL_H
