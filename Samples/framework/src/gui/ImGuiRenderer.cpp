/*
 * Copyright 2024 - 2024 PICO. All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of PICO.
 * The intellectual and technical concepts contained herein are proprietary to PICO.
 * and may be covered by patents, patents in process, and are protected by trade
 * secret or copyright law. Dissemination of this information or reproduction of
 * this material is strictly forbidden unless prior written permission is obtained
 * from PICO.
 *//*
 * Copyright 2024 - 2024 PICO. All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of PICO.
 * The intellectual and technical concepts contained herein are proprietary to PICO.
 * and may be covered by patents, patents in process, and are protected by trade
 * secret or copyright law. Dissemination of this information or reproduction of
 * this material is strictly forbidden unless prior written permission is obtained
 * from PICO.
 */

#include "ImGuiRenderer.h"
#include "LogUtils.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

namespace PVRSampleFW {
    void ImGuiRenderer::Initialize(void *context) {
        if (initialized_)
            return;

        auto *glContext = static_cast<GuiGLContext *>(context);

        display_ = glContext->display;
        context_ = glContext->context;

        if (!render_in_xr_loop_) {
            thread_ = std::thread(&ImGuiRenderer::InitForMultiThread, this);
            return;
        }

        initialized_ = true;
    }

    void ImGuiRenderer::Shutdown() {
        if (render_in_xr_loop_) {
            std::unique_lock<std::mutex> lock(mutex_);
            initialized_ = false;
            condition_.notify_one();
            thread_.join();
        } else {
            Destroy();
        }
    }

    void ImGuiRenderer::TriggerSignal() {
        if (render_in_xr_loop_) {
            RenderAllWindows();
            return;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.notify_one();
    }

    uint32_t ImGuiRenderer::AddWindow(const std::shared_ptr<GuiWindow> &window) {
        gui_window_map_.emplace(++window_num_, window);
        return window_num_;
    }

    int ImGuiRenderer::InitForContext(EGLDisplay displayParm, EGLContext contextParm) {
        if (initialized_)
            return 0;

        if (displayParm == EGL_NO_DISPLAY) {
            PLOGE("ImGuiRenderer Init failed for display_ is EGL_NO_DISPLAY");
            return -1;
        }
        display_ = displayParm;

        EGLBoolean ret = eglInitialize(display_, 0, 0);
        if (ret != EGL_TRUE) {
            PLOGE("ImGuiRenderer Init failed for eglInitialize returned with an error=%d", ret);
            return -1;
        }

        const EGLint eglAttributes[] = {EGL_BLUE_SIZE,  8,  EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
                                        EGL_DEPTH_SIZE, 24, EGL_NONE};
        EGLint numConfigs = 0;
        ret = eglChooseConfig(display_, eglAttributes, nullptr, 0, &numConfigs);
        if (ret != EGL_TRUE) {
            PLOGE("ImGuiRenderer Init failed for eglChooseConfig returned with an error=%d", ret);
            return -1;
        }

        if (numConfigs == 0) {
            PLOGE("ImGuiRenderer Init failed for eglChooseConfig returned 0 matching config");
            return -1;
        }

        // Get the first matching config
        EGLConfig eglConfig;
        eglChooseConfig(display_, eglAttributes, &eglConfig, 1, &numConfigs);

        const EGLint egl_context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context_ = eglCreateContext(display_, eglConfig, contextParm, egl_context_attributes);

        if (context_ == EGL_NO_CONTEXT) {
            PLOGE("ImGuiRenderer Init failed for eglCreateContext returned EGL_NO_CONTEXT");
            return -1;
        }

        ret = eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_);
        if (ret != EGL_TRUE) {
            PLOGE("ImGuiRenderer Init failed for eglMakeCurrent returned with an error=%d", ret);
            return -1;
        }

        return 0;
    }

    ImGuiRenderer::~ImGuiRenderer() {
        if (initialized_) {
            Shutdown();
        }
    }

    void ImGuiRenderer::ConfigImGuiDisplay(uint32_t width, uint32_t height) {
        ImGuiIO &io = ImGui::GetIO();

        int32_t window_width = width;
        int32_t window_height = height;
        int display_width = window_width;
        int display_height = window_height;

        io.DisplaySize = ImVec2(static_cast<float>(window_width), static_cast<float>(window_height));
        if (window_width > 0 && window_height > 0) {
            io.DisplayFramebufferScale = ImVec2(static_cast<float>(display_width) / window_width,
                                                static_cast<float>(display_height) / window_height);
        }
    }

    void ImGuiRenderer::ConfigRenderTarget(RenderTarget *renderTarget, const WindowConfig &config) {
        PLOGW("ImGuiRenderer::configRenderTarget begin");
        glGenTextures(SWAP_BUFFER_COUNT, &renderTarget->textureId);
        glGenFramebuffers(SWAP_BUFFER_COUNT, &renderTarget->framebufferId);

        for (int i = 0; i < SWAP_BUFFER_COUNT; ++i) {
            //            GLuint &texture = texture_ids_[i];
            GLuint &texture = renderTarget->textureId;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderTarget->width, renderTarget->height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            //            GLuint &fbo = framebuffer_ids_[i];
            GLuint &fbo = renderTarget->framebufferId;
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                PLOGE("ImGuiRenderer Init failed for framebuffer not complete, status=%d", status);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        renderTarget->imGuiContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(renderTarget->imGuiContext);
        ImGui::StyleColorsDark();
        //        ImGui::StyleColorsLight();
        //        ImGui::StyleColorsClassic();
        ImGui_ImplOpenGL3_Init("#version 300 es");
        ImGuiIO &io = ImGui::GetIO();

        default_font_size_ = config.flags & GUI_WINDOW_CONFIG_FONT_SIZE ? config.textSize : DEFAULT_FONT_SIZE;
        const int index = (default_font_size_ - MIN_FONT_SIZE) / FONT_SIZE_INTERVAL;
        ImFontConfig fontConfig;
        fontConfig.SizePixels = static_cast<float>(default_font_size_);
        renderTarget->font[index] = io.Fonts->AddFontDefault(&fontConfig);

        for (int i = 0; i < FONT_SIZE_NUM; ++i) {
            checkNewFont(renderTarget, MIN_FONT_SIZE + i * FONT_SIZE_INTERVAL);
        }

        ImGui::GetStyle().ScaleAllSizes(3.0f);
        PLOGW("ImGuiRenderer::configRenderTarget end");
    }

    void ImGuiRenderer::checkNewFont(RenderTarget *renderTarget, int fontSize) {
        ImGuiIO &io = ImGui::GetIO();
        auto font = renderTarget->font;
        if (fontSize < MIN_FONT_SIZE || fontSize > MAX_FONT_SIZE) {
            PLOGE("ImGuiRenderer::checkNewFont fontSize(%f) is invalid, use default size %d", fontSize,
                  default_font_size_);
            return;
        }

        const int index = (fontSize - MIN_FONT_SIZE) / FONT_SIZE_INTERVAL;
        if (font[index] == nullptr) {
            ImFontConfig fontConfig;
            fontConfig.SizePixels = static_cast<float>(fontSize);
            font[index] = io.Fonts->AddFontDefault(&fontConfig);
        }
    }

    ImFont *ImGuiRenderer::GetFont(RenderTarget *renderTarget, int fontSize) {
        ImGuiIO &io = ImGui::GetIO();
        auto font = renderTarget->font;
        const int index = (fontSize - MIN_FONT_SIZE) / FONT_SIZE_INTERVAL;
        if (fontSize < MIN_FONT_SIZE || fontSize > MAX_FONT_SIZE || font[index] == nullptr) {
            PLOGE("ImGuiRenderer::GetFont fontSize(%f) is invalid, use default size %d", fontSize, default_font_size_);
            return font[(default_font_size_ - MIN_FONT_SIZE) / FONT_SIZE_INTERVAL];
        }

        return font[index];
    }

    void ImGuiRenderer::InitForMultiThread() {
        if (initialized_)
            return;

        int ret = 0;
        if (!render_in_xr_loop_) {
            ret = InitForContext(display_, context_);
            if (ret != 0)
                return;
        }

        initialized_ = true;

        while (initialized_) {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock);
            PLOGD("notify one");

            Destroy();
            PLOGW("ImGuiRenderer::initForMultiThread end");
        }
    }

    void ImGuiRenderer::RenderAllWindows() {
        for (const auto &[idx, window] : gui_window_map_) {
            auto config = window->GetConfig();
            auto it = render_target_map_.find(idx);
            if (it == render_target_map_.end()) {
                RenderTarget renderTarget = {
                        .width = config.width,
                        .height = config.height,
                };
                ConfigRenderTarget(&renderTarget, config);
                render_target_map_[idx] = renderTarget;
            }
            RenderOneWindow(window, &render_target_map_[idx]);
        }
    }

    void ImGuiRenderer::RenderOneWindow(const std::shared_ptr<GuiWindow> &window, RenderTarget *renderTarget) {
        auto rayInputEvent = window->GetCurrentRayInputEvent();
        if (!window->IsRendered()) {
            return;
        }

        ImGui::SetCurrentContext(renderTarget->imGuiContext);
        ImGuiIO &io = ImGui::GetIO();
        auto config = window->GetConfig();

        // 1. Start the Window ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ConfigImGuiDisplay(config.width, config.height);
        ImGui::NewFrame();

        // 2. update input for two ray
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        // The y-axis of the screen coordinate system is opposite to
        // that of the OpenGL ES coordinate system.
        /// NOTE: only single ray event can be handled because of limitation of imgui
        auto effectiveSide = rayInputEvent.effective_side;
        io.AddMousePosEvent(rayInputEvent.coord_pixel[effectiveSide].first,
                            config.height - rayInputEvent.coord_pixel[effectiveSide].second);
        io.AddMouseButtonEvent(0, rayInputEvent.triggered);

        // 3. window size
        if (config.flags & GUI_WINDOW_CONFIG_WINDOW_SIZE) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(config.width, config.height));
        } else {
            ImVec2 text_size = ImGui::CalcTextSize(config.text.c_str(), NULL, false, TEXT_WIDTH_PIXEL);
            float window_width = text_size.x + HORIZONTAL_MARGIN_PIXEL;
            float window_height = text_size.y + VERTICAL_MARGIN_PIXEL;
            ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
        }

        // 4. title
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        if (config.flags & GUI_WINDOW_CONFIG_BG_COLOR) {
            ImVec4 backgroundColor = ImVec4(config.bgColor[0], config.bgColor[1], config.bgColor[2], config.bgColor[3]);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColor);
        } else {
            windowFlags |= ImGuiWindowFlags_NoBackground;
        }
        if ((config.flags & GUI_WINDOW_CONFIG_TITLE) != GUI_WINDOW_CONFIG_TITLE) {
            windowFlags |= ImGuiWindowFlags_NoTitleBar;
        }
        if (config.flags & GUI_WINDOW_CONFIG_NO_SCROLLBAR) {
            windowFlags |= ImGuiWindowFlags_NoScrollbar;
        }
        ImGui::Begin(config.title.c_str(), nullptr, windowFlags);

        if (config.flags & GUI_WINDOW_CONFIG_TEXT_COLOR) {
            ImVec4 textColor =
                    ImVec4(config.textColor[0], config.textColor[1], config.textColor[2], config.textColor[3]);
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        }

        // 5. primary text
        if (config.flags & GUI_WINDOW_CONFIG_TEXT) {
            ImGui::TextWrapped("%s", config.text.c_str());
        }

        // 6. render components
        auto components = window->GetComponents();
        for (const auto &[id, component] : components) {
            int pushStyleColorCount = 0;
            ImVec2 originalCursorPos = ImGui::GetCursorPos();
            if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_POS) {
                ImGui::SetCursorPos(ImVec2(component.posX, component.posY));
            }
            if (component.flags & GUI_WINDOW_CONFIG_TEXT_COLOR) {
                ImVec4 textColor = ImVec4(component.textColor[0], component.textColor[1], component.textColor[2],
                                          component.textColor[3]);
                ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                pushStyleColorCount++;
            }
            if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_FONT_SIZE) {
                ImGui::PushFont(GetFont(renderTarget, component.textSize));
            }
            switch (component.componentType) {
            case GUI_WINDOW_COMPONENT_TYPE_BUTTON: {
                if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_BG_COLOR) {
                    ImVec4 backgroundColor = ImVec4(component.bgColor[0], component.bgColor[1], component.bgColor[2],
                                                    component.bgColor[3]);
                    ImGui::PushStyleColor(ImGuiCol_Button, backgroundColor);
                    pushStyleColorCount++;
                }
                bool bCallback{false};
                if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_SIZE) {
                    bCallback = ImGui::Button(component.name.c_str(), ImVec2(component.width, component.height));
                } else {
                    bCallback = ImGui::Button(component.name.c_str());
                }

                if (bCallback) {
                    (component.buttonCallback)();
                }
                break;
            }
            case GUI_WINDOW_COMPONENT_TYPE_CHECKBOX: {
                if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_BG_COLOR) {
                    ImVec4 backgroundColor = ImVec4(component.bgColor[0], component.bgColor[1], component.bgColor[2],
                                                    component.bgColor[3]);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, backgroundColor);
                    pushStyleColorCount++;
                }
                ImGui::Checkbox(component.name.c_str(), component.checked);
                PLOGD("check box value=%d", *component.checked);
                break;
            }
            case GUI_WINDOW_COMPONENT_TYPE_TEXT: {
                ImGui::TextWrapped("%s", component.name.c_str());
                break;
            }
            default: {
                PLOGW("ImGuiRenderer::RenderOneWindow unknown component type(%d)", component.componentType);
                break;
            }
            }

            if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_POS) {
                ImGui::SetCursorPos(originalCursorPos);
            }
            if (pushStyleColorCount > 0) {
                ImGui::PopStyleColor(pushStyleColorCount);
            }
            if (component.flags & GUI_WINDOW_CONFIG_COMPONENT_FONT_SIZE) {
                ImGui::PopFont();
            }
        }

        if (config.flags & GUI_WINDOW_CONFIG_TEXT_COLOR) {
            ImGui::PopStyleColor();
        }
        if (config.flags & GUI_WINDOW_CONFIG_BG_COLOR) {
            ImGui::PopStyleColor();
        }
        ImGui::End();

        ImGui::Render();

        // 3. Rendering
        glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->framebufferId);

        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 4. Draw point for two ray
        GLboolean lastEnableScissorTest = glIsEnabled(GL_SCISSOR_TEST);
        if (lastEnableScissorTest == GL_FALSE) {
            glEnable(GL_SCISSOR_TEST);
        }
        for (int i = 0; i < 2; ++i) {
            if (rayInputEvent.collision[i]) {
                glScissor(rayInputEvent.coord_pixel[i].first - POINT_SIZE,
                          rayInputEvent.coord_pixel[i].second - POINT_SIZE, POINT_SIZE * 2, POINT_SIZE * 2);
                glClearColor(0.9f, 0.9f, 0.8f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        if (lastEnableScissorTest == GL_FALSE) {
            glDisable(GL_SCISSOR_TEST);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        window->SetTextureId(renderTarget->textureId);
    }

    void ImGuiRenderer::Destroy() {
        for (auto [idx, renderTarget] : render_target_map_) {
            glDeleteFramebuffers(1, &renderTarget.framebufferId);
            glDeleteTextures(1, &renderTarget.textureId);
            renderTarget.framebufferId = 0;
            renderTarget.textureId = 0;

            ImGui::DestroyContext(renderTarget.imGuiContext);
            renderTarget.imGuiContext = nullptr;
        }
        render_target_map_.clear();
        gui_window_map_.clear();

        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display_, context_);
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            context_ = EGL_NO_CONTEXT;
        }
    }

}  // namespace PVRSampleFW
