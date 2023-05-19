#include "mutation_control.hpp"

#include <time.h>

#include "imgui.h"
#include "render/shader.hpp"
#include "data.hpp"

uint32_t u32_upk(uint32_t u32, uint32_t bts, uint32_t off)
{
    return (u32 >> off) & ((1u << bts) - 1u);
}

MutationControl::MutationControl(std::string fragment_name)
: _fragment_name{fragment_name}
{
    _seed = time(NULL);
    for (size_t i = 0; i < 64; i++)
    {
        _mutation.push_back(0);
    }
    _mutation[0] = 1534876570;
    _mutation[1] = 3102187896;
    _mutation[2] = 383180533;
    _mutation[3] = 3911937888;
    _mutation[4] = 1713008546;
    _mutation[5] = 3183668977;
    _mutation[6] = 163787620;
    _mutation[7] = 1570659573;
    _mutation[8] = 4275609029;
    _mutation[9] = 1409283414;
    _mutation[10] = 1517509157;
    _mutation[11] = 4185999792;
    _mutation[12] = 1785950172;
    _mutation[13] = 1839866699;
    _mutation[14] = 3464288272;
    _mutation[15] = 894531137;
    _mutation[16] = 931560489;
    _mutation[17] = 2107890683;
    _mutation[18] = 69319769;
    _mutation[19] = 2430523639;
    _mutation[20] = 769976078;
    _mutation[21] = 2822674395;
    _mutation[22] = 2653587597;
    _mutation[23] = 1512850439;
    _mutation[24] = 1053467775;
    _mutation[25] = 2833240997;
    _mutation[26] = 1075074238;
    _mutation[27] = 1129631768;
    _mutation[28] = 1352606717;
    _mutation[29] = 3469685548;
    _mutation[30] = 2802655431;
    _mutation[31] = 2318177844;
    _mutation[32] = 3425762429;
    _mutation[33] = 543502637;
    _mutation[34] = 2736492674;
    _mutation[35] = 3676755342;
    _mutation[36] = 1248651794;
    _mutation[37] = 1223860922;
    _mutation[38] = 943980818;
    _mutation[39] = 330461460;
    _mutation[40] = 1717743274;
    _mutation[41] = 2286157824;
    _mutation[42] = 295261525;
    _mutation[43] = 1713547946;
    _mutation[44] = 2339061161;
    _mutation[45] = 2680507410;
    _mutation[46] = 3285330708;
    // _mutation[47] = 2831848434;
    // mutation[48] = 0;
    // mutation[49] = 0;
    // mutation[50] = 0;
    // mutation[51] = 0;
    // mutation[52] = 0;
    // mutation[53] = 0;
    // mutation[54] = 0;
    // mutation[55] = 0;
    // mutation[56] = 0;
    // mutation[57] = 0;
    // mutation[58] = 0;
    // mutation[59] = 0;
    // mutation[60] = 0;
    // _mutation[61] = 1083179006;
    // _mutation[62] = 1116152287;
    // _mutation[63] = 1174405259;
    for (size_t i = 0; i < 24 * 4; i++)
    {
        uint32_t scale = _mutation[12 + i / 4];
        uint8_t scale_i = u32_upk(scale, 8u, i * 8u);
        _mutation_scale.push_back(scale_i);
    }
    _mutation_save = _mutation;
}

MutationControl::~MutationControl()
{
}

void draw_col_button(int &id, uint32_t &state, uint32_t off, int col, bool same_line = true)
{
    ImGui::PushID(id++);
    uint32_t selected = state & (3u << off);
    ImVec4 col_vec{0.f, 0.f, 0.f, 1.f};
    col_vec.x = col == 0 || col == 3 ? 0.9 : 0.2;
    col_vec.y = col == 1 || col == 3 ? 0.9 : 0.2;
    col_vec.z = col == 2 || col == 3 ? 0.9 : 0.2;
    if (selected == col << off)
        ImGui::PushStyleColor(ImGuiCol_Button, col_vec);
    else
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.3f, 0.3f, 1.0f});
    if (ImGui::Button("", ImVec2(10, 10)))
    {
        state -= state & (3u << off);
        state += col << off;
    }
    ImGui::PopStyleColor(1);
    ImGui::PopID();
    if (same_line)
        ImGui::SameLine();
}

void draw_button(int &id, uint32_t &state, uint32_t off, bool same_line = false)
{
    ImGui::PushID(id++);
    uint32_t selected = state & (1 << off);
    if (selected)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.5f, 0.8f, 1.0f});
    else
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.3f, 0.3f, 1.0f});
    if (ImGui::Button("", ImVec2(10, 10)))
    {
        state = state ^ (1 << off);
    }
    ImGui::PopStyleColor(1);
    ImGui::PopID();
    if (same_line)
        ImGui::SameLine();
}

void draw_slider(int &id, uint8_t *state, bool same_line = false)
{
    const uint8_t min_scale = 0;
    const uint8_t max_scale = 255;
    ImGui::PushID(id++);
    ImGui::SliderScalar("", ImGuiDataType_U8, state, &min_scale, &max_scale, "%u");
    ImGui::PopID();
    if (same_line)
        ImGui::SameLine();
}

uint32_t u32_flp(uint32_t u32, uint32_t off) { return u32 ^ (1 << off); }
uint32_t u32_set(uint32_t u32, uint32_t off) { return u32 | (1 << off); }
uint32_t u32_clr(uint32_t u32, uint32_t off) { return u32 & (1 << off); }

uint32_t mut_rnd() { return rand() % UINT32_MAX; }

uint32_t blk_clr(uint32_t u32)
{
    for (int i = 0; i < 32; i++)
    {
        u32 = u32_clr(u32, i);
    }
    return u32;
}

uint32_t blk_set(uint32_t u32)
{
    for (int i = 0; i < 32; i++)
    {
        u32 = u32_set(u32, i);
    }
    return u32;
}

uint32_t wrd_clr(uint32_t u32, uint32_t off, uint32_t len)
{
    len = len - (((off + len) / 32) * ((off + len) % 32));
    for (int i = off; i < off + len; i++)
    {
        u32 = u32_set(u32, i);
    }
    return u32;
}

uint32_t wrd_set(uint32_t u32, uint32_t off, uint32_t len)
{
    len = len - (((off + len) / 32) * ((off + len) % 32));
    for (int i = off; i < off + len; i++)
    {
        u32 = u32_set(u32, i);
    }
    return u32;
}

uint32_t wrd_flp(uint32_t u32, uint32_t off, uint32_t len)
{
    len = len - (((off + len) / 32) * ((off + len) % 32));
    for (int i = off; i < off + len; i++)
    {
        u32 = u32_flp(u32, i);
    }
    return u32;
}

uint32_t bit_flp(uint32_t u32, int rnd)
{
    for (int i = 0; i < 32; i++)
    {
        if (rand() % rnd == 0)
        {
            u32 = u32_flp(u32, i);
        }
    }
    for (int i = 0; i < 32; i++)
    {
        if (rand() % rnd == 0)
        {
            u32 = u32_flp(u32, i);
        }
    }
    for (int i = 0; i < 32; i++)
    {
        if (rand() % rnd == 0)
        {
            u32 = u32_flp(u32, i);
        }
    }
    for (int i = 0; i < 32; i++)
    {
        if (rand() % rnd == 0)
        {
            u32 = u32_flp(u32, i);
        }
    }
    for (int i = 0; i < 32; i++)
    {
        if (rand() % rnd == 0)
        {
            u32 = u32_flp(u32, i);
        }
    }
    if (rand() % (rnd * 2) == 0)
    {
        u32 = wrd_set(u32, rand() % 32, rand() % 8);
    }
    if (rand() % (rnd * 2) == 0)
    {
        u32 = wrd_clr(u32, rand() % 32, rand() % 8);
    }
    if (rand() % (rnd * 2) == 0)
    {
        u32 = wrd_flp(u32, rand() % 32, rand() % 8);
    }
    return u32;
}

void MutationControl::draw_mutation_window()
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(-1, 600));
    ImGui::Begin("Mutation controls", &_show_mutation_window, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("%d", _frames);

    ImGui::SliderFloat("Scale", &_scale, 0.0f, 200.0f);
    ImGui::SliderFloat("Zoom", &_zoom, 0.0f, 1.0f);
    ImGui::SliderInt("Mode", &_mode, 0, 2);
    ImGui::SliderInt("Chance", &_mutation_chance, 10, 500);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    int id = 0;
    for (int y = 0; y < 24; y++)
    {
        for (int x = 0; x < 16; x++)
        {
            if (x > 0)
                ImGui::SameLine();
            int nhx = x + (y % 2) * 16;
            int nhy = y / 2;
            draw_button(id, _mutation[nhy], nhx);
        }
        const uint8_t min_scale = 0;
        const uint8_t max_scale = 255;
        ImGui::PushItemWidth(200);
        for (int x = 0; x < 2; x++)
        {
            uint32_t used = _mutation_scale[y * 4 + x * 2] <= _mutation_scale[y * 4 + x * 2 + 1];
            draw_button(id, used, 0, true);
            draw_slider(id, &_mutation_scale[y * 4 + x * 2], true);
            draw_slider(id, &_mutation_scale[y * 4 + x * 2 + 1], true);
            int usy = 36 + y / 16;
            int usx = (y * 2 + x) % 32;
            draw_button(id, _mutation[usy], usx, true);
            {
                int chy = 38 + y / 8;
                int chx = (y * 4) % 32;
                draw_col_button(id, _mutation[chy], chx, 0);
                draw_col_button(id, _mutation[chy], chx, 1);
                draw_col_button(id, _mutation[chy], chx, 2);
                draw_col_button(id, _mutation[chy], chx, 3);
            }
            {
                int chy = 41 + y / 8;
                int chx = (y * 4) % 32;
                draw_col_button(id, _mutation[chy], chx, 0);
                draw_col_button(id, _mutation[chy], chx, 1);
                draw_col_button(id, _mutation[chy], chx, 2);
                draw_col_button(id, _mutation[chy], chx, 3);
            }
            {
                int chy = 44 + y / 8;
                int chx = (y * 4) % 32;
                draw_col_button(id, _mutation[chy], chx, 0);
                draw_col_button(id, _mutation[chy], chx, 1);
                draw_col_button(id, _mutation[chy], chx, 2);
                draw_col_button(id, _mutation[chy], chx, 3, false);
            }

            // if (x % 2 == 0)
            //     ImGui::SameLine();
        }
        ImGui::PopItemWidth();
        ImGui::Separator();
    }
    for (size_t i = 0; i < 24; i++)
    {
        _mutation[12 + i] = 0u;
    }
    for (size_t i = 0; i < 24 * 4; i++)
    {
        _mutation[12 + i / 4] += _mutation_scale[i] << 8u * i;
    }

    ImGui::End();
}

void MutationControl::draw()
{
    if (ImGui::IsKeyPressed('Q'))
    {
        _reset = true;
        _frames = 0;
    }
    if (ImGui::IsKeyPressed('P'))
    {
        _clear = true;
    }
    if (ImGui::IsKeyPressed('S'))
    {
        _mutation_save = _mutation;
    }
    if (ImGui::IsKeyPressed('L'))
    {
        _mutation = _mutation_save;
        _reset = true;
        _frames = 0;
    }

    if (ImGui::IsKeyPressed('V'))
    {
        for (size_t i = 0; i < 48; i++)
        {
            _mutation[i] = bit_flp(_mutation[i], _mutation_chance);
        }
        _reset = true;
    }
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Mutation"))
        {
            if (ImGui::MenuItem("Mutation control", "..."))
            {
                _show_mutation_window = !_show_mutation_window;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset", "Q"))
            {
                _reset = true;
                _frames = 0;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear", "P"))
            {
                _clear = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Mutate", "V"))
            {
                for (size_t i = 0; i < 48; i++)
                {
                    _mutation[i] = bit_flp(_mutation[i], _mutation_chance);
                }
                _reset = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save mutation", "S"))
            {
                _mutation_save = _mutation;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load mutation", "L"))
            {
                _mutation = _mutation_save;
                _reset = true;
                _frames = 0;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
    for (size_t i = 0; i < 24 * 4; i++)
    {
        uint32_t scale = _mutation[12 + i / 4];
        uint8_t scale_i = u32_upk(scale, 8u, i * 8u);
        _mutation_scale[i] = scale_i;
    }

    if (_show_mutation_window)
    {
        draw_mutation_window();
    }
}

void MutationControl::update(Data& data)
{
    auto& program_data = data.get_element<FragmentProgramData>(_fragment_name);
    auto& program = program_data.get_program();
    program.use();

    ImGuiIO &io = ImGui::GetIO();
    ImVec2 screen_size = io.DisplaySize;
    ImVec2 mouse_pos = io.MousePos;
    mouse_pos.x = mouse_pos.x / screen_size.x;
    mouse_pos.y = 1. - (mouse_pos.y / screen_size.y);

    bool left_click = ImGui::IsMouseDown(0);
    bool right_click = ImGui::IsMouseDown(1);
    if (io.WantCaptureMouse)
    {
        left_click = false;
        right_click = false;
    }
    program.set_uniform("nb", 12, &_mutation[0]);
    program.set_uniform("ur", 24, &_mutation[12]);
    program.set_uniform("us", 2, &_mutation[36]);
    program.set_uniform("ch", 3, &_mutation[38]);
    program.set_uniform("ch2", 3, &_mutation[41]);
    program.set_uniform("ch3", 3, &_mutation[44]);
    program.set_uniform("mxy", mouse_pos.x, mouse_pos.y);
    program.set_uniform("mlr", left_click, right_click);
    program.set_uniform("mode", (uint32_t)_mode);
    if (_reset)
        program.set_uniform("cmd", (uint32_t)1);
    else if (_clear)
        program.set_uniform("cmd", (uint32_t)2);
    else
        program.set_uniform("cmd", (uint32_t)0);
    program.set_uniform("zoom", _zoom);
    program.set_uniform("scale", _scale);
    program.set_uniform("frames", _frames);
    program.set_uniform("rand_seed", _seed % (1 << 16));
    // program->set_uniform("stage", (uint32_t)frames % 4);
    _reset = false;
    _clear = false;
    _frames++;
    _seed++;

    glUseProgram(0);
}