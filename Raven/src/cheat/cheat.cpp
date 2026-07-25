#include "cheat.h"
#include "../offsets/offsets.h"

#include <Windows.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>

#include "imgui.h"
#include "../gui/gui.h"

namespace raven::cheat {

namespace {

struct Vec3
{
    float x, y, z;
};

struct Quat
{
    float x, y, z, w;
};

std::atomic<bool> running{ false };
std::atomic<uintptr_t> player{ 0 };
std::atomic<uintptr_t> position{ 0 };
std::thread locator_thread;

namespace bools
{
    std::atomic<bool> running{ false };
    std::atomic<uintptr_t> player{ 0 };
    std::atomic<uintptr_t> position{ 0 };
    std::thread locator_thread;

    bool fly = false;
    bool speed_boost = false;
    bool high_jump = false;
    float fly_speed = 550.0f;
    float speed_value = 1600.0f;
    float jump_value = 900.0f;
    float original_speed = 550.0f;
    float original_jump = 420.0f;
    float original_gravity = 1.0f;
    bool defaults_captured = false;
    uintptr_t defaults_player = 0;
    std::atomic<bool> wall_step_requested{ false };
}

bool readable(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
        return false;
    const auto start = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    const auto end = start + mbi.RegionSize;
    return mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
           address >= start && address + size <= end;
}

template <typename T>
bool read_value(uintptr_t address, T& output) {
    if (!readable(address, sizeof(T)))
        return false;
    __try {
        output = *reinterpret_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool write_value(uintptr_t address, const T& value) {
    if (!readable(address, sizeof(T)))
        return false;
    __try {
        *reinterpret_cast<T*>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool validate(uintptr_t candidate, uintptr_t module, uintptr_t& out_position) {
    uintptr_t vtable{}, pos{}, owner{}, pos_owner{}, owner_vtable{}, pos_vtable{};
    Vec3 xyz{}, cached{};
    if (!read_value(candidate, vtable) || vtable != module + offsets::player_vtable ||
        !read_value(candidate + offsets::owner, owner) ||
        !read_value(candidate + offsets::position_component, pos) ||
        !read_value(pos + offsets::owner, pos_owner) || owner != pos_owner ||
        !read_value(owner, owner_vtable) || owner_vtable != module + offsets::owner_vtable ||
        !read_value(pos, pos_vtable) || pos_vtable != module + offsets::position_vtable ||
        !read_value(pos + offsets::world_position, xyz) ||
        !read_value(candidate + offsets::cached_position, cached))
        return false;

    if (!std::isfinite(xyz.x) || !std::isfinite(xyz.y) || !std::isfinite(xyz.z))
        return false;

    if (std::fabs(xyz.x) + std::fabs(xyz.y) + std::fabs(xyz.z) < 1.0f)
        return false;

    if (std::fabs(xyz.x - cached.x) > 2000.0f ||
        std::fabs(xyz.y - cached.y) > 2000.0f ||
        std::fabs(xyz.z - cached.z) > 2000.0f)
        return false;

    out_position = pos;
    return true;
}

bool resolve_player(uintptr_t module, uintptr_t& out_player,
                            uintptr_t& out_position) {
    uintptr_t candidate{};
    if (!read_value(module + offsets::player_pointer_base, candidate) || !candidate)
        return false;

    constexpr auto count =
        sizeof(offsets::player_pointer_offsets) /
        sizeof(offsets::player_pointer_offsets[0]);

    for (size_t i = 0; i + 1 < count; ++i) {
        uintptr_t next{};
        if (!read_value(candidate + offsets::player_pointer_offsets[i], next) || !next)
            return false;
        candidate = next;
    }
    candidate += offsets::player_pointer_offsets[count - 1];

    uintptr_t resolved_position{};
    if (!validate(candidate, module, resolved_position))
        return false;

    out_player = candidate;
    out_position = resolved_position;
    return true;
}

void locate_loop() {
    const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"HelloNeighbor-Win64-Shipping.exe"));

    while (running) {
        uintptr_t resolved_player{};
        uintptr_t resolved_position{};
        if (module && resolve_player(module, resolved_player, resolved_position)) {
            player = resolved_player;
            position = resolved_position;
        } else {
            player = 0;
            position = 0;
        }
        Sleep(100);
    }
}

void restore_defaults() {
    if (!bools::defaults_player || !bools::defaults_captured) return;
    write_value(bools::defaults_player + offsets::gravity_scale, bools::original_gravity);
    write_value(bools::defaults_player + offsets::jump_power, bools::original_jump);
    write_value(bools::defaults_player + offsets::movement_speed, bools::original_speed);
}

void get_defaults(uintptr_t p) {
    if (bools::defaults_captured && bools::defaults_player == p) return;
    if (bools::defaults_captured && bools::defaults_player != p) restore_defaults();
    bools::defaults_captured = false;
    bools::defaults_player = 0;
    if (read_value(p + offsets::gravity_scale, bools::original_gravity) &&
        read_value(p + offsets::jump_power, bools::original_jump) &&
        read_value(p + offsets::movement_speed, bools::original_speed)) {
        bools::defaults_captured = true;
        bools::defaults_player = p;
    }
}

void facing(uintptr_t pos, float& fx, float& fy, float& rx, float& ry) {
    Quat q{};
    if (!read_value(pos + offsets::rotation, q)) {
        fx = 1.0f; fy = 0.0f; rx = 0.0f; ry = 1.0f; return;
    }
    fx = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    fy = 2.0f * (q.x * q.y + q.w * q.z);
    const float length = std::hypot(fx, fy);
    if (length < 0.01f) { fx = 1.0f; fy = 0.0f; }
    else { fx /= length; fy /= length; }
    rx = -fy; ry = fx;
    }
}

void initialize() {
    if (running.exchange(true)) return;
    locator_thread = std::thread(locate_loop);
}

void shutdown() {
    restore_defaults();
    running = false;
    if (locator_thread.joinable()) locator_thread.join();
}

void tick() {
    if (GetAsyncKeyState('5') & 1) bools::fly = !bools::fly;
    if (GetAsyncKeyState('6') & 1) bools::speed_boost = !bools::speed_boost;
    if (GetAsyncKeyState('7') & 1) bools::high_jump = !bools::high_jump;
    if (GetAsyncKeyState('8') & 1) bools::wall_step_requested = true;

    const auto p = player.load();
    const auto pos = position.load();
    if (!p || !pos) return;
    get_defaults(p);

    write_value(p + offsets::movement_speed, bools::speed_boost ? bools::speed_value : bools::original_speed);
    write_value(p + offsets::jump_power, bools::high_jump ? bools::jump_value : bools::original_jump);

    if (bools::wall_step_requested.exchange(false)) {
        Vec3 xyz{};
        float fx{}, fy{}, rx{}, ry{};
        if (read_value(pos + offsets::world_position, xyz)) {
            facing(pos, fx, fy, rx, ry);
            xyz.x += fx * 350.0f;
            xyz.y += fy * 350.0f;
            write_value(pos + offsets::world_position, xyz);
        }
    }

    if (!bools::fly) {
        write_value(p + offsets::gravity_scale, bools::original_gravity);
        return;
    }

    write_value(p + offsets::gravity_scale, 0.0f);
    float fx{}, fy{}, rx{}, ry{};
    facing(pos, fx, fy, rx, ry);
    Vec3 velocity{};

    if (GetAsyncKeyState('W') & 0x8000) { velocity.x += fx; velocity.y += fy; }
    if (GetAsyncKeyState('S') & 0x8000) { velocity.x -= fx; velocity.y -= fy; }
    if (GetAsyncKeyState('D') & 0x8000) { velocity.x += rx; velocity.y += ry; }
    if (GetAsyncKeyState('A') & 0x8000) { velocity.x -= rx; velocity.y -= ry; }

    const float length = std::hypot(velocity.x, velocity.y);
    const float speed = bools::fly_speed * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 2.0f : 1.0f);
    if (length > 0.01f) {
        velocity.x = velocity.x / length * speed;
        velocity.y = velocity.y / length * speed;
    }

    if (GetAsyncKeyState(VK_SPACE) & 0x8000) velocity.z += speed;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) velocity.z -= speed;

    write_value(p + offsets::velocity, velocity);
}

void render_menu() {
    ImGui::SetNextWindowSize(ImVec2(390, 350), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Raven")) { ImGui::End(); return; }

    const bool ready = player.load() && position.load();

    ImGui::Checkbox("Fly [5]", &bools::fly);
    ImGui::SliderFloat("Fly speed", &bools::fly_speed, 100.0f, 2000.0f, "%.0f");
    ImGui::Checkbox("Speed boost [6]", &bools::speed_boost);
    ImGui::SliderFloat("Speed", &bools::speed_value, 550.0f, 2500.0f, "%.0f");
    ImGui::Checkbox("High jump [7]", &bools::high_jump);
    ImGui::SliderFloat("Jump power", &bools::jump_value, 420.0f, 1600.0f, "%.0f");

    if (ImGui::Button("Move 7 Steps [8]", ImVec2(-1.0f, 0.0f)))bools::wall_step_requested = true;
    ImGui::Separator();

    ImGui::TextUnformatted("Insert: toggle menu");
    ImGui::Text("Delete: mouse %s", mouseUnlocked ? "unlocked" : "locked");

    if (ImGui::Button(mouseUnlocked ? "Lock mouse" : "Unlock mouse", ImVec2(-1.0f, 0.0f))) mouseUnlocked = !mouseUnlocked;

    ImGui::TextUnformatted("End: restore values and unload");

    ImGui::End();
    }
}
