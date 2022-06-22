#pragma once
#include "../bin_serializer/bin_serializer.h"

// Keep this file synchronized between server and client!

#define PACKET_MAGIC		'FI00'
#define PACKET_BUFFER_SIZE	4096	// Temporary buffer size for recv

#pragma pack(push, 1)

namespace fi::packets {
	enum flags {
		fl_none				= 0,
		fl_handshake_cl		= ( 1 << 0 ),
		fl_handshake_sv		= ( 1 << 1 ),
		fl_heartbeat		= ( 1 << 2 ),
		fl_disconnect		= ( 1 << 3 )

		// Put your custom packet flags here
	};

	enum ids {
		id_none = 0,
		id_handshake,
		id_heartbeat,
		id_disconnect,
		
		num_preset_ids,

		// Put your custom packet IDs here
		id_example
	};

	// Do NOT remove/change any of these unless you know what you're doing!
	struct header {
		std::uint32_t magic		= PACKET_MAGIC;
		std::uint16_t id		= ids::id_none;
		std::uint16_t flags		= flags::fl_none;
		std::uint32_t length	= sizeof( header );
	};

	typedef decltype( header::id ) packet_id;
	typedef decltype( header::flags ) packet_flags;
	typedef decltype( header::length ) packet_length;

	// Each packet must be based off this class
	class base_packet {
	public:
		// Override these three methods (example shown in packets.h)
		virtual void serialize( detail::binary_serializer& s ) = 0;
		virtual void deserialize( detail::binary_serializer& s ) = 0;

		virtual packet_id get_id( ) = 0;
	};

} // namespace fi::packets

#pragma pack(pop)