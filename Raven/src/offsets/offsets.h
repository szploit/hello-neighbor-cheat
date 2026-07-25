#pragma once

#include <cstdint>

namespace offsets {
	inline constexpr std::uintptr_t player_vtable = 0x22F4758;
	inline constexpr std::uintptr_t position_vtable = 0x22F3DD0;
	inline constexpr std::uintptr_t owner_vtable = 0x1F1A250;

	inline constexpr std::uintptr_t player_pointer_base = 0x02D3EAD0;
	inline constexpr std::uintptr_t player_pointer_offsets[] = { 0x2A0, 0x140, 0x20, 0x390, 0x0 };

	inline constexpr std::uintptr_t owner = 0x20;
	inline constexpr std::uintptr_t position_component = 0xF0;
	inline constexpr std::uintptr_t velocity = 0x104;
	inline constexpr std::uintptr_t gravity_scale = 0x1A4;
	inline constexpr std::uintptr_t step_height = 0x1A8;
	inline constexpr std::uintptr_t jump_power = 0x1AC;
	inline constexpr std::uintptr_t movement_speed = 0x1E4;
	inline constexpr std::uintptr_t cached_position = 0x2A4;

	inline constexpr std::uintptr_t rotation = 0x180;
	inline constexpr std::uintptr_t world_position = 0x190;
	inline constexpr std::uintptr_t reported_velocity = 0x1B0;
}
