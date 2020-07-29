#pragma once
#include <cstdint>
#include <vector>
#include <string>

#define ARITHMETIC_TYPE_ONLY typename std::enable_if< std::is_arithmetic< T >::value >::type* = nullptr

namespace fi::packets::detail {
	class binary_serializer {
	public:
		// Methods for serializiation
		template < typename T, ARITHMETIC_TYPE_ONLY >
		void serialize( T item ) {
			write_to_buffer( item );
		}

		template < typename T, ARITHMETIC_TYPE_ONLY >
		void serialize( std::vector< T >& item ) {
			write_to_buffer< std::uint32_t >( item.size( ) );
			
			serialized_buffer_.insert( 
				serialized_buffer_.end( ), 
				reinterpret_cast< std::uint8_t* >( item.data( ) ),
				reinterpret_cast< std::uint8_t* >( item.data( ) ) + item.size( ) * sizeof( T )
			);
		}

		void serialize( const std::string& item );
		void serialize( const std::vector< std::string >& item );

		// Methods for deserialization
		template < typename T, ARITHMETIC_TYPE_ONLY >
		void deserialize( T& item ) {
			item = read_from_buffer< T >( );
		}

		template < typename T, ARITHMETIC_TYPE_ONLY >
		void deserialize( std::vector< T >& out_item ) {
			auto num_items = read_from_buffer< std::uint32_t >( );

			out_item.resize( num_items );
			memcpy( out_item.data( ), serialized_buffer_.data( ) + deserialized_bytes_, num_items * sizeof( T ) );

			deserialized_bytes_ += num_items * sizeof( T );
		}

		void deserialize( std::string& out_item );
		void deserialize( std::vector< std::string >& out_item );

		std::uint8_t* get_serialized_data( );
		std::uint32_t get_serialized_data_length( );

		void reset( );
		void assign_buffer( std::uint8_t* const data, std::uint32_t length );

	private:
		template < typename T, ARITHMETIC_TYPE_ONLY >
		void write_to_buffer( T item ) {
			serialized_buffer_.insert( 
				serialized_buffer_.end( ), 
				reinterpret_cast< std::uint8_t* >( &item ),
				reinterpret_cast< std::uint8_t* >( &item ) + sizeof( T )
			);
		}

		template < typename T, ARITHMETIC_TYPE_ONLY >
		T read_from_buffer( ) {
			auto value = *reinterpret_cast< T* >( serialized_buffer_.data( ) + deserialized_bytes_ );
			deserialized_bytes_ += sizeof( T );
			
			return value;
		}

		std::uint32_t deserialized_bytes_ = 0;
		std::vector< std::uint8_t > serialized_buffer_ = { };
	};
}