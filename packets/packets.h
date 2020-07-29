#pragma once
#include "packet_base.h"

/*
	Allowed types for members:

	base_types:
	int8, uint8
	int16, uint16
	int32, uint32
	int64, uint64

	arrays:
	std::vector< base_types >
	std::vector< std::string >

	Any other combination should not be used.
	Remember to use platform-independent types (base_types, cstdint.h)
	to make sure everything works fine across different architectures.
*/

namespace fi::packets {
	class example_packet : public base_packet {
	public:
		// If you want to, you can implement a custom constructor for your members,
		// but it is not needed.
		example_packet( ) { }

		example_packet( detail::binary_serializer& s ) {
			deserialize( s );
		}

		virtual void serialize( detail::binary_serializer& s ) {
			// Serialize our items here
			s.serialize( some_short );
			s.serialize( some_array );
			s.serialize( some_string_array );
		}

		virtual void deserialize( detail::binary_serializer& s ) {
			// Deserialize our items here.
			// IMPORTANT: Deserialize them in the same order you serialized them in!
			s.deserialize( some_short );
			s.deserialize( some_array );
			s.deserialize( some_string_array );
		}
	
		virtual packet_id get_id( ) {
			return ids::id_example;
		}

		// Make your members public to be able to access them.
		std::uint16_t some_short = 0;
		std::vector< std::uint8_t > some_array = { };
		std::vector< std::string > some_string_array = { };
	};
}