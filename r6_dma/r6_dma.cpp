#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <random>
#include <thread>
#include <chrono>
#include "../vmread/hlapi/hlapi.h"
#include "offsets.h"
#include "config.h"
#include "../glm/glm.hpp"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static bool run_cheat = true;

uint64_t get_base(WinProcess &proc)
{
	PEB peb = proc.GetPeb();
	return peb.ImageBaseAddress;
}

void read_data(WinProcess &proc, R6Data &data, bool init = true)
{
	if (init)
	{
		data.base = get_base(proc);
	}

	auto localplayer = proc.Read<uint64_t>(data.base + PROFILE_MANAGER_OFFSET);
	localplayer = proc.Read<uint64_t>(localplayer + 0x68);
	localplayer = proc.Read<uint64_t>(localplayer + 0x0);
	localplayer = proc.Read<uint64_t>(localplayer + 0x28) - 0x1454F66EFA8B8713;
	data.local_player = localplayer;

	auto fov_manager = proc.Read<uint64_t>(data.base + FOV_MANAGER_OFFSET);
	fov_manager = proc.Read<uint64_t>(fov_manager + 0xE8);
	fov_manager = proc.Read<uint64_t>(fov_manager + 0x88B932A0D99755B8);
	data.fov_manager = fov_manager;

	auto weapon = proc.Read<uint64_t>(localplayer + 0x90);
	weapon = proc.Read<uint64_t>(weapon + 0xc8);
	data.curr_weapon = weapon;

	auto weapon2 = proc.Read<uint64_t>(data.curr_weapon + 0x290) - 0x2b306cb952f73b96;
	data.weapon_info = weapon2;

	data.glow_manager = proc.Read<uint64_t>(data.base + GLOW_MANAGER_OFFSET);

	data.round_manager = proc.Read<uint64_t>(data.base + ROUND_MANAGER_OFFSET);

	data.game_manager = proc.Read<uint64_t>(data.base + GAME_MANAGER_OFFSET); 
}

void enable_esp(WinProcess &proc, const R6Data &data)
{
	if(!USE_CAV_ESP) return;

	if(data.game_manager == 0) return;
	auto entity_list = proc.Read<uint64_t>(data.game_manager + 0x98) + 0xE60F6CF8784B5E96;
	if(entity_list == 0) return;
	for(int i = 0; i < 11; ++i)
	{
		auto entity_address = proc.Read<uint64_t>(entity_list + (0x8 * i)); //entity_object
		auto buffer = proc.Read<uint64_t>(entity_address + 0x18);
		auto size = proc.Read<uint32_t>(buffer + 0xE0) & 0x3FFFFFFF;
		auto list_address = proc.Read<uint64_t>(buffer + 0xD8);
		for(uint32_t i = 0; i < size; ++i)
		{
			auto pbuffer = proc.Read<uint64_t>(list_address + i * sizeof(uint64_t));
			auto current_vtable_rel = proc.Read<uint64_t>(pbuffer) - data.base;
			if(current_vtable_rel == VTMARKER_OFFSET)
			{
				auto spotted = proc.Read<uint8_t>(pbuffer + 0x632);
				if(!spotted)
				{
					proc.Write<uint8_t>(pbuffer + 0x632, 1);
				}
			}
		}
	}
	printf("Cav esp active\n");
}

void enable_no_recoil(WinProcess &proc, const R6Data &data)
{
	if(!USE_NO_RECOIL) return;

	if(data.fov_manager == 0) return;
	//proc.Write<float>(data.weapon_info + 0x198, 0.f); //rec b
    uint64_t _u1 = proc.Read<uint64_t>(data.game_manager + 0x2468);
    uint64_t _u2 = proc.Read<uint64_t>(_u1 + 0x138);
    uint64_t _u3 = proc.Read<uint64_t>(_u2);
    proc.Write<int>(_u3 + 0x40, 20);
    //proc.Write<float>(0x23f4b92384c, 0.85);
	//printf("SEX active\n");
}

void enable_no_spread(WinProcess &proc, const R6Data &data)
{
	if(!USE_NO_SPREAD) return;

	if(data.weapon_info == 0) return;
	proc.Write<float>(data.weapon_info + 0x80, 0.f); // no spread
	printf("No spread active\n");
}

void enable_run_and_shoot(WinProcess &proc, const R6Data &data)
{
	if(!USE_RUN_AND_SHOOT) {
		printf("SEX IS NOT HAPPENIGN!!!\n");
		return;
	}
	if(data.base == 0) return;
	proc.Write<uint8_t>( data.base + 0x33AE195 + 0x6, 0x1 );    // always sprint =   80 B9 80 00 00 00 ?? 74 15 E8     | ?? == set to 1/0
	proc.Write<uint8_t>( data.base + 0x23C571F + 0x6, 0x1 );    // fix viewbobbing = 80 B9 80 00 00 00 ?? 75 52        |
	proc.Write<uint8_t>( data.base + 0x1EA5E24 + 0x6, 0x1 );    // enable lean =     80 B9 80 00 00 00 ?? 75 18 48     |
	proc.Write<uint8_t>( data.base + 0x1E58EA7 + 0x6, 0x1 );    // notarget =        80 B9 80 00 00 00 ?? 74 45 49     |
	proc.Write<uint8_t>( data.base + 0x3BD9AC5 + 0x6, 0x1 );    // fix viewbobbing = 80 B9 80 00 00 00 ?? 74 6D        |
	proc.Write<uint8_t>( data.base + 0x1E59401 + 0x6, 0x1 );    // shootanytime =    80 B9 80 00 00 00 ?? 0F 84 F4     |
	proc.Write<uint16_t>( data.base + 0x1AC992C + 0x3, 0x460 ); // silentaim =       0F 28 97 ?? ?? 00 00 0F 28 87     | 0xc0->0x460

	//printf("Run and shoot active\n");
}

void enable_no_flash(WinProcess &proc, const R6Data &data)
{
	if(!USE_NO_FLASH) return;

	if(data.local_player == 0) return;
	auto player = proc.Read<uint64_t>(data.local_player + 0x30);
	player = proc.Read<uint64_t>(player + 0x31);
	auto noflash = proc.Read<uint64_t>(player + 0x28);
	proc.Write<uint8_t>(noflash + 0x40, 0); // noflash
	printf("Noflash active\n");
}

void enable_no_aim_animation(WinProcess &proc, const R6Data &data)
{
	if(!USE_NO_AIM_ANIM) return;

	if(data.local_player == 0) return;
	auto no_anim = proc.Read<uint64_t>(data.local_player + 0x90);
	no_anim = proc.Read<uint64_t>(no_anim + 0xc8);
	proc.Write<uint8_t>(no_anim + 0x384, 0);
}

void set_fov(WinProcess &proc, const R6Data &data, float fov_val)
{
	if(!CHANGE_FOV) return;

	auto fov = proc.Read<uint64_t>(data.base + FOV_MANAGER_OFFSET);
	if(fov == 0) return;
	fov = proc.Read<uint64_t>(fov + 0x60) + 0xe658f449242c196;
	if(fov == 0) return;
	auto playerfov = proc.Read<uint64_t>(fov + 0x0) + 0x38;
	if(playerfov == 0) return;
	proc.Write<float>(playerfov, fov_val); // player fov ~1.2f-2.3f
	printf("Player fov changed to %f\n", fov_val);
}

void set_firing_mode(WinProcess &proc, const R6Data &data, FiringMode mode)
{
	if(!CHANGE_FIRING_MODE) return;

	if(data.curr_weapon == 0) return; 
	proc.Write<uint32_t>(data.curr_weapon + 0x118, (uint32_t)mode); //firing mode 0 - auto 3 - single 2 -  burst
	printf("Fire mode: %s\n", (mode == FiringMode::AUTO ? "auto" : mode == FiringMode::BURST ? "burst" : "single"));
}

void set_speed ( WinProcess& proc , const R6Data& data )
{
    if ( !USE_SPEED )
    {
        return;
    }


    const uint64_t _r1 = proc.Read<uint64_t> ( data.local_player + 0x30 );

    if ( !_r1 ) {
        return;
    }

    const uint64_t _r2 = proc.Read<uint64_t> ( _r1 + 0x31 );

    if ( !_r2 ) {
        return;
    }

    const uint64_t _r3 = proc.Read<uint64_t> ( _r2 + 0x38 );

    if ( !_r3 ) {
        return;
    }


	proc.Write<int> ( _r3 + 0x58 , 185 );


}

void enable_glow(WinProcess &proc, const R6Data &data)
{
	if(!USE_GLOW) return;
	const auto glow = proc.Read<uint64_t>( data.glow_manager + 0xb8 );
	if (!glow) return;
	//if (proc.Read<int>( glow + 0xE4 )) return;
	proc.Write<int>( glow + 0xE4, 0 ); // 0 == darkmode, 1 == fullbright
	printf("SEX!!\n");
}

int32_t get_game_state(WinProcess &proc, const R6Data &data)
{
	if(data.round_manager == 0) return -1;
	return proc.Read<uint8_t>(data.round_manager + 0x2e8); // 3/2=in game 1=in operator selection menu 5=in main menu
}

//player is currently in operator selection menu
bool is_in_op_select_menu(WinProcess &proc, const R6Data& data)
{
	return get_game_state(proc, data) == 1;
}

bool is_in_main_menu(WinProcess &proc, const R6Data& data)
{
	return get_game_state(proc, data) == 5;
}

bool is_in_game(WinProcess &proc, const R6Data& data)
{
	return !is_in_op_select_menu(proc, data) && !is_in_main_menu(proc, data) && get_game_state(proc, data) != 0;
}

void unlock_all(WinProcess &proc, const R6Data& data)
{
	if(!UNLOCK_ALL) return;
	uint8_t shellcode[] = { 65, 198, 70, 81, 0, 144 };
	proc.WriteMem(data.base + 0x271470B, shellcode, sizeof(shellcode));
	printf("Unlock all executed\n");
}
/*
constexpr auto r2d = 57.2957795131f;
constexpr auto d2r = 0.01745329251f; 
constexpr auto pi = 3.14159265358979323846f;
static glm::vec3 ToEuler( glm::vec4 q )
{
	glm::vec3 end = glm::vec3( );

	float sinr = ( float )( +2.0 * ( q.w * q.x + q.y * q.z ) );
	float cosr = ( float )( +1.0 - 2.0 * ( q.x * q.x + q.y * q.y ) );
	end.z = ( float )atan2( sinr, cosr );

	double sinp = +2.0 * ( q.w * q.y - q.z * q.x );
	if ( abs( sinp ) >= 1 )
		end.x = ( float )copysign( pi / 2, sinp );
	else
		end.x = ( float )asin( sinp );

	double siny = +2.0 * ( q.w * q.z + q.x * q.y );
	double cosy = +1.0 - 2.0 * ( q.y * q.y + q.z * q.z );
	end.y = ( float )atan2( siny, cosy );

	return end;
}

static float GetDifference( float firstAngle, float secondAngle )
{
	float difference = secondAngle - firstAngle;
	while ( difference < -180.f ) difference += 360.f;
	while ( difference > 180.f ) difference -= 360.f;
	return difference;
}

static glm::vec4 QuaternionFromYPR( float yaw, float pitch, float roll )
{
	glm::vec4 q;
	double cy = cos( yaw * 0.5 );
	double sy = sin( yaw * 0.5 );
	double cr = cos( pitch * 0.5 );
	double sr = sin( pitch * 0.5 );
	double cp = cos( roll * 0.5 );
	double sp = sin( roll * 0.5 );

	q.w = ( float )( cy * cr * cp + sy * sr * sp );
	q.x = ( float )( cy * sr * cp - sy * cr * sp );
	q.y = ( float )( cy * cr * sp + sy * sr * cp );
	q.z = ( float )( sy * cr * cp - cy * sr * sp );
	return q;
}

static glm::vec3 VectorAngles( glm::vec3 forward )
{
	glm::vec3 angles;
	float tmp, yaw, pitch;

	if ( forward.y == 0 && forward.y == 0 )
	{
		yaw = 0;
		if ( forward.y > 0 )
			pitch = 270;
		else
			pitch = 90;
	}
	else
	{
		yaw = ( float )( atan2( forward.y, forward.x ) * 180 / pi );

		tmp = ( float )sqrt( forward.x * forward.x + forward.y * forward.y );
		pitch = ( float )( atan2( forward.z * -1, tmp ) * 180 / pi );
	}

	yaw += 90;

	if ( yaw > 180 )
	{
		yaw -= 360;
	}
	if ( pitch > 180 )
	{
		pitch -= 360;
	}

	angles.x = pitch;
	angles.y = yaw;
	angles.z = 0;
	return angles;
}

static glm::vec4 calc_angle( glm::vec3 viewTranslation, glm::vec3 enemyHead, glm::vec4 originalAngle, float smoothamount )
{
	glm::vec3 aimAngle = VectorAngles( viewTranslation - enemyHead );
	glm::vec3 currentAngle = ToEuler( originalAngle );
	currentAngle *= ( float )( 180.f / pi );
	glm::vec3 currentAngleV = glm::vec3( currentAngle.z, currentAngle.y, currentAngle.x );

	glm::vec3 smoothedAngle = glm::vec3( aimAngle.x, aimAngle.y, aimAngle.z );
	smoothedAngle.x = GetDifference( currentAngleV.x, smoothedAngle.x );
	smoothedAngle.y = GetDifference( currentAngleV.y, smoothedAngle.y );
	smoothedAngle *= ( smoothamount * 0.1f );
	currentAngleV.x += smoothedAngle.x;
	currentAngleV.y += smoothedAngle.y;

	if ( currentAngleV.x > 89.0f && currentAngleV.x <= 180.0f )
	{
		currentAngleV.x = 89.0f;
	}
	while ( currentAngleV.x > 180.f )
	{
		currentAngleV.x -= 360.f;
	}
	while ( currentAngleV.x < -89.0f )
	{
		currentAngleV.x = -89.0f;
	}
	while ( currentAngleV.y > 180.f )
	{
		currentAngleV.y -= 360.f;
	}
	while ( currentAngleV.y < -180.f )
	{
		currentAngleV.y += 360.f;
	}

	aimAngle *= ( float )( pi / 180.f );
	currentAngle *= ( float )( pi / 180.f );
	currentAngleV *= ( float )( pi / 180.f );

	glm::vec4 newQuaternion = QuaternionFromYPR( currentAngleV.y, currentAngleV.x, 0 );
	return newQuaternion;
}

auto world_to_screen( glm::vec3& world, glm::vec2* screen, WinProcess &proc, const R6Data &data ) -> bool
{

	auto profile_manager = proc.Read<uint64_t>( proc.Read<uint64_t>( proc.Read<uint64_t>( data.base + 0x653ED48 ) + 0x68 ) );
	if ( !profile_manager )
		return false;

	auto camholder = proc.Read<uint64_t>( profile_manager + 0x180 );
	if ( !camholder )
		return false;

	auto matrix = proc.Read<uint64_t>( camholder + 0x410 );

	const auto tmp = world - proc.Read<glm::vec3>( matrix + 0x7f0 );

	const float x = glm::dot( tmp, proc.Read<glm::vec3>( matrix + 0x7c0 ) ); //right
	const float y = glm::dot( tmp, proc.Read<glm::vec3>( matrix + 0x7d0 ) ); //up
	const float z = glm::dot( tmp, proc.Read<glm::vec3>( matrix + 0x7e0 ) * -1.f ); //forward

	*screen =
	{
		( 2560.f / 2.f ) * ( 1.f + x / std::abs( proc.Read<float>( matrix + 0x800 ) ) / z ),//fovx
		( 1440.f / 2.f ) * ( 1.f - y / std::abs( proc.Read<float>( matrix + 0x814 ) ) / z ),//fovy
	};

	return z >= 1.0f ? true : false;
}

auto get_local( WinProcess &proc, const R6Data &data ) -> uint64_t
{
	const auto profile_manager = proc.Read<uint64_t>( data.base + 0x653ED48 );
	if ( !profile_manager )
		return 0;

	const auto container = proc.Read<uint64_t>( profile_manager + 0x78 );
	if ( !container )
		return 0;

	const auto deref = proc.Read<uint64_t>( container );
	if ( !deref )
		return 0;

	const auto pawn = proc.Read<uint64_t>( deref + 0x28 ) - 0x1454F66EFA8B8713;
	return pawn;
}

auto get_best_ent( WinProcess &proc, const R6Data &data ) -> uint64_t
{
	if( !data.game_manager ) 
		return 0;

	const auto entity_list = proc.Read<uint64_t>(data.game_manager + 0x98) + 0xE60F6CF8784B5E96;
	if ( !entity_list )
		return 0;

	float best_fov = 100.f;
	float closest;

	uint64_t return_entity{ 0 };

	auto local_container = get_local( proc, data );
	if ( !local_container )
		return 0;

	auto local = proc.Read<uint64_t>( local_container + 0x20 ) + 0xB9A25DD8A6AD943D;
	if ( !local )
		return 0;

	for ( auto idx = 0u; idx < 12; idx++ )
	{
		auto entity = proc.Read<uint64_t>( entity_list + ( 0x8 * idx ) );
		if ( !entity || local == entity )
			continue;

		auto head = proc.Read<glm::vec3>( entity + 0xFC0 );

		auto health = proc.Read<int>( proc.Read<uint64_t>( proc.Read<uint64_t>( proc.Read<uint64_t>( entity + 0x18 ) + 0xd8 ) + 0x8 ) + 0x168 );
		if ( health <= 0 )
			continue;

		glm::vec2 head_out;
		if ( !world_to_screen( head, &head_out, proc, data ) )
			continue;

		const auto first = fabsf( 2560 / 2 - head_out.x );
		const auto second = fabsf( 1440 / 2 - head_out.y );
		closest = first + second;

		if ( closest < best_fov )
		{
			best_fov = closest;
			return_entity = entity;
		}
	}

	return return_entity;
}

void aimbot( WinProcess &proc, R6Data &data )
{
	
	auto local_container = get_local( proc, data );
	if ( !local_container )
		return;

	auto local = proc.Read<uint64_t>( local_container + 0x20 ) + 0xB9A25DD8A6AD943D;
	if ( !local )
		return;

	auto angle_ptr = proc.Read<uint64_t>( local + 0x11f0 );
	if ( !angle_ptr )
		return;

	glm::vec4 angles = proc.Read<glm::vec4>( angle_ptr + 0x460 );

	auto profile_manager = proc.Read<uint64_t>( proc.Read<uint64_t>( proc.Read<uint64_t>( data.base + 0x653ED48 ) + 0x68 ) );
	if ( !profile_manager )
		return;

	auto camholder = proc.Read<uint64_t>( profile_manager + 0x180 );
	if ( !camholder )
		return;

	auto best_entity = get_best_ent( proc, data );
	if ( !best_entity )
		return;

	auto head = proc.Read<glm::vec3>( best_entity + 0xFC0 );

	auto translation = proc.Read<glm::vec3>( proc.Read<uint64_t>( camholder + 0x410 ) + 0x7f0 );
	auto calced = calc_angle( translation, head, angles, 6.f );


	proc.Write<glm::vec4>( angle_ptr + 0x460, calced );

}*/

void update_all(WinProcess &proc, R6Data &data, ValuesUpdates& update)
{

	if(update.update_cav_esp)
	{
		enable_esp(proc, data);
		update.update_cav_esp = false;
		printf("Esp updated\n");
	}
	if(update.update_no_recoil)
	{
		enable_no_recoil(proc, data);
		update.update_no_recoil = false;
		//printf("No recoil updated\n");
	}
	if(update.update_no_spread)
	{
		enable_no_spread(proc, data);
		update.update_no_spread = false;
		printf("No spread updated\n");
	}	
	if(update.update_no_flash)
	{
		enable_no_flash(proc, data);
		update.update_no_flash = false;
		printf("No flash updated\n");
	}
	if(update.update_firing_mode)
	{
		set_firing_mode(proc, data, CURRENT_FIRE_MODE);
		update.update_firing_mode = false;
		printf("Firing mode updated\n");
	}
	if(update.update_run_and_shoot)
	{
		//printf("Run and shoot sex\n");
		enable_run_and_shoot(proc, data);
		update.update_run_and_shoot = false;
		//printf("Run and shoot sex\n");
	}
	if(update.update_fov)
	{
		set_fov(proc, data, NEW_FOV);
		update.update_fov = false;
		printf("Fov updated\n");
	}
	
	if(update.update_speed)
	{
		set_speed(proc, data);
		update.update_speed = false;
		//printf("Speed updated\n");
	}
	enable_no_aim_animation(proc, data);
	enable_glow(proc, data);
	//aimbot(proc, data);
}

void check_update(WinProcess &proc, R6Data &data, ValuesUpdates& update)
{
	static bool esp_updated = false;

	if(USE_RUN_AND_SHOOT) {
		//printf("sex sex sex\n");
		update.update_run_and_shoot = true;
	}

	if(USE_SPEED) {
		//printf("sex sex sex\n");
		update.update_speed = true;
	}
	
	if(USE_CAV_ESP)
	{
		if(esp_updated)
		{
			if(!is_in_game(proc, data))
			{
				esp_updated = false;
			}
		}
		if(!esp_updated && is_in_game(proc, data))
		{
			update.update_cav_esp = true;
			esp_updated = true;
		}
	}
	if(USE_NO_SPREAD)
	{
		float spread_val = proc.Read<float>(data.weapon_info + 0x80); 
		if(spread_val)
		{
			update.update_no_spread = true;
		}
	}
	if(USE_NO_RECOIL)
	{
		update.update_no_recoil = true;
	}
	if(USE_NO_FLASH)
	{
		auto player = proc.Read<uint64_t>(data.local_player + 0x30);
		player = proc.Read<uint64_t>(player + 0x31);
		auto noflash = proc.Read<uint64_t>(player + 0x28);
		auto noflash_value = proc.Read<uint8_t>(noflash + 0x40);
		if(noflash_value)
		{
			update.update_no_flash = true;
		}
	}	
	if(CHANGE_FIRING_MODE)
	{
		uint32_t firing_mode = proc.Read<uint32_t>(data.curr_weapon + 0x118);
		if(firing_mode != (uint32_t)CURRENT_FIRE_MODE)
		{
			update.update_firing_mode = true;
		}
	}
	if(CHANGE_FOV)
	{
		auto fov = proc.Read<uint64_t>(data.base + FOV_MANAGER_OFFSET);
		fov = proc.Read<uint64_t>(fov + 0x60) + 0xe658f449242c196;
		auto playerfov = proc.Read<uint64_t>(fov + 0x0) + 0x38;
		auto fov_value = proc.Read<float>(playerfov); 
		if(fov_value != NEW_FOV)
		{
			update.update_fov = true;
		}
	}
}

void setLevel(WinProcess &proc, R6Data &data) {

	if (!data.profile_manager) return;

	const uint64_t _b1 = proc.Read<uint64_t>( data.profile_manager + 0x68 ); 

	const uint64_t _b2 = proc.Read<uint64_t>( _b1 + 0x0 );

	proc.Write<int> ( _b2 + 0x610 ,  200 );	

}

void write_loop(WinProcess &proc, R6Data &data)
{
	printf("Write thread started\n\n");
	ValuesUpdates update = {false,false,false,false,false,false,false};
	while (run_cheat)
	{
		read_data(proc, data, 
			data.base == 0
			|| data.fov_manager == 0
			|| data.curr_weapon == 0
			|| data.game_manager == 0 
			|| data.glow_manager == 0
			|| data.round_manager == 0
			|| data.weapon_info == 0);

		//setLevel(proc, data);
			
		while((is_in_main_menu(proc, data)) 
				|| get_game_state(proc, data) == 0) //waiting until the game is started
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		check_update(proc, data, update);

		if(is_in_game(proc, data))
		{			
			update_all(proc, data, update);			
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	printf("Exiting...\n");
}



__attribute__((constructor)) static void init()
{
	pid_t pid = 0;
#if (LMODE() != MODE_EXTERNAL())
	pid = getpid();
#endif
	printf("Using Mode: %s\n", TOSTRING(LMODE));

	try
	{
		R6Data data;
		bool not_found = true;
		WinContext write_ctx(pid);
		WinContext check_ctx(pid);
		printf("Searching for a process...\n");
		while (not_found)
		{
			write_ctx.processList.Refresh();
			for (auto &i : write_ctx.processList)
			{
				if (!strcasecmp("RainbowSix.exe", i.proc.name))
				{
					short magic = i.Read<short>(i.GetPeb().ImageBaseAddress);
					auto peb = i.GetPeb();
					auto g_Base = peb.ImageBaseAddress;
					if (g_Base != 0)
					{
						printf("\nR6 found %lx:\t%s\n", i.proc.pid, i.proc.name);
						printf("\tBase:\t%lx\tMagic:\t%hx (valid: %hhx)\n", peb.ImageBaseAddress, magic, (char)(magic == IMAGE_DOS_SIGNATURE));
						printf("Press enter to start\n");
						getchar();
						read_data(i, data);

						printf("Base: 0x%lx\nFOV Manager: 0x%lx\nLocal player: 0x%lx\nWeapon: 0x%lx\nGlow manager: 0x%lx\nRound manager: 0x%lx\nGame manager: 0x%lx\n\n", 
								data.base, data.fov_manager, data.local_player, data.curr_weapon, data.glow_manager, data.round_manager, data.game_manager);

						if(get_game_state(i, data) == 5)
						{
							unlock_all(i, data);
						}
   					 	set_fov(i, data, NEW_FOV);
   					 	

						std::thread write_thread(write_loop, std::ref(i), std::ref(data));
						write_thread.detach();

						not_found = false;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		while (run_cheat)
		{
			bool flag = false;
			check_ctx.processList.Refresh();
			for (auto &i : check_ctx.processList)
			{
				if (!strcasecmp("RainbowSix.exe", i.proc.name))
				{
					auto peb = i.GetPeb();
					auto g_Base = peb.ImageBaseAddress;
					if (g_Base != 0)
					{
						flag = true;
						break;
					}
				}
			}
			if (!flag)
				run_cheat = false;

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (VMException &e)
	{
		printf("Initialization error: %d\n", e.value);
	}
}

int main()
{
	return 0;
}