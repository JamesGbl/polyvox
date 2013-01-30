/*******************************************************************************
Copyright (c) 2005-2009 David Williams

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution. 	
*******************************************************************************/

#include "PolyVoxCore/Impl/ErrorHandling.h"
#include "PolyVoxCore/Impl/Utility.h"

#include "PolyVoxCore/MinizCompressor.h"
#include "PolyVoxCore/Vector.h"

#include "PolyVoxCore/Impl/ErrorHandling.h"

#include <cstring> //For memcpy
#include <limits>
#include <stdexcept> //for std::invalid_argument

namespace PolyVox
{
	template <typename VoxelType>
	Block<VoxelType>::Block(uint16_t uSideLength)
		:m_pCompressedData(0)
		,m_uCompressedDataLength(0)
		,m_tUncompressedData(0)
		,m_uSideLength(0)
		,m_uSideLengthPower(0)
		,m_bIsCompressed(false)
		,m_bIsUncompressedDataModified(true)
	{
		if(uSideLength != 0)
		{
			initialise(uSideLength);
		}
	}

	template <typename VoxelType>
	uint16_t Block<VoxelType>::getSideLength(void) const
	{
		return m_uSideLength;
	}

	template <typename VoxelType>
	VoxelType Block<VoxelType>::getVoxelAt(uint16_t uXPos, uint16_t uYPos, uint16_t uZPos) const
	{
		POLYVOX_ASSERT(uXPos < m_uSideLength, "Supplied position is outside of the block");
		POLYVOX_ASSERT(uYPos < m_uSideLength, "Supplied position is outside of the block");
		POLYVOX_ASSERT(uZPos < m_uSideLength, "Supplied position is outside of the block");

		POLYVOX_ASSERT(m_tUncompressedData, "No uncompressed data - block must be decompressed before accessing voxels.");

		return m_tUncompressedData
			[
				uXPos + 
				uYPos * m_uSideLength + 
				uZPos * m_uSideLength * m_uSideLength
			];
	}

	template <typename VoxelType>
	VoxelType Block<VoxelType>::getVoxelAt(const Vector3DUint16& v3dPos) const
	{
		return getVoxelAt(v3dPos.getX(), v3dPos.getY(), v3dPos.getZ());
	}

	template <typename VoxelType>
	void Block<VoxelType>::setVoxelAt(uint16_t uXPos, uint16_t uYPos, uint16_t uZPos, VoxelType tValue)
	{
		POLYVOX_ASSERT(uXPos < m_uSideLength, "Supplied position is outside of the block");
		POLYVOX_ASSERT(uYPos < m_uSideLength, "Supplied position is outside of the block");
		POLYVOX_ASSERT(uZPos < m_uSideLength, "Supplied position is outside of the block");

		POLYVOX_ASSERT(m_tUncompressedData, "No uncompressed data - block must be decompressed before accessing voxels.");

		m_tUncompressedData
		[
			uXPos + 
			uYPos * m_uSideLength + 
			uZPos * m_uSideLength * m_uSideLength
		] = tValue;

		m_bIsUncompressedDataModified = true;
	}

	template <typename VoxelType>
	void Block<VoxelType>::setVoxelAt(const Vector3DUint16& v3dPos, VoxelType tValue)
	{
		setVoxelAt(v3dPos.getX(), v3dPos.getY(), v3dPos.getZ(), tValue);
	}

	template <typename VoxelType>
	void Block<VoxelType>::initialise(uint16_t uSideLength)
	{
		//Debug mode validation
		POLYVOX_ASSERT(isPowerOf2(uSideLength), "Block side length must be a power of two.");

		//Release mode validation
		if(!isPowerOf2(uSideLength))
		{
			POLYVOX_THROW(std::invalid_argument, "Block side length must be a power of two.");
		}

		//Compute the side length		
		m_uSideLength = uSideLength;
		m_uSideLengthPower = logBase2(uSideLength);

		//Create the block data
		m_tUncompressedData = new VoxelType[m_uSideLength * m_uSideLength * m_uSideLength];

		//Clear it (should we bother?)
		const uint32_t uNoOfVoxels = m_uSideLength * m_uSideLength * m_uSideLength;
		std::fill(m_tUncompressedData, m_tUncompressedData + uNoOfVoxels, VoxelType());
		m_bIsUncompressedDataModified = true;

		//For some reason blocks start out compressed. We should probably change this.
		compress();
	}

	template <typename VoxelType>
	uint32_t Block<VoxelType>::calculateSizeInBytes(void)
	{
		uint32_t uSizeInBytes = sizeof(Block<VoxelType>);
		uSizeInBytes += m_vecCompressedData.capacity() * sizeof(RunlengthEntry<uint16_t>);
		return  uSizeInBytes;
	}

	template <typename VoxelType>
	void Block<VoxelType>::compress(void)
	{
		POLYVOX_ASSERT(m_bIsCompressed == false, "Attempted to compress block which is already flagged as compressed.");
		POLYVOX_ASSERT(m_tUncompressedData != 0, "No uncompressed data is present.");

		//If the uncompressed data hasn't actually been
		//modified then we don't need to redo the compression.
		if(m_bIsUncompressedDataModified)
		{
			void* pSrcData = reinterpret_cast<void*>(m_tUncompressedData);
			void* pDstData = reinterpret_cast<void*>( new uint8_t[1000000] );
			uint32_t uSrcLength = m_uSideLength * m_uSideLength * m_uSideLength * sizeof(VoxelType);
			uint32_t uDstLength = 1000000;

			MinizCompressor compressor;
			uint32_t uCompressedLength = compressor.compress(pSrcData, uSrcLength, pDstData, uDstLength);

			m_pCompressedData = reinterpret_cast<void*>( new uint8_t[uCompressedLength] );
			memcpy(m_pCompressedData, pDstData, uCompressedLength);
			m_uCompressedDataLength = uCompressedLength;

			delete pDstData;


			/*Data src;
			src.ptr = reinterpret_cast<uint8_t*>(m_tUncompressedData);
			src.length = m_uSideLength * m_uSideLength * m_uSideLength * sizeof(VoxelType);

			Data compressedResult = polyvox_compress(src);

			m_pCompressedData = compressedResult.ptr;
			m_uCompressedDataLength = compressedResult.length;*/

			/*uint32_t uNoOfVoxels = m_uSideLength * m_uSideLength * m_uSideLength;
			m_vecCompressedData.clear();

			RunlengthEntry<uint16_t> entry;
			entry.length = 1;
			entry.value = m_tUncompressedData[0];

			for(uint32_t ct = 1; ct < uNoOfVoxels; ++ct)
			{		
				VoxelType value = m_tUncompressedData[ct];
				if((value == entry.value) && (entry.length < entry.maxRunlength()))
				{
					entry.length++;
				}
				else
				{
					m_vecCompressedData.push_back(entry);
					entry.value = value;
					entry.length = 1;
				}
			}

			m_vecCompressedData.push_back(entry);

			//Shrink the vectors to their contents (maybe slow?):
			//http://stackoverflow.com/questions/1111078/reduce-the-capacity-of-an-stl-vector
			//C++0x may have a shrink_to_fit() function?
			std::vector< RunlengthEntry<uint16_t> >(m_vecCompressedData).swap(m_vecCompressedData);*/
		}

		//Flag the uncompressed data as no longer being used.
		delete[] m_tUncompressedData;
		m_tUncompressedData = 0;
		m_bIsCompressed = true;
	}

	template <typename VoxelType>
	void Block<VoxelType>::uncompress(void)
	{
		POLYVOX_ASSERT(m_bIsCompressed == true, "Attempted to uncompress block which is not flagged as compressed.");
		POLYVOX_ASSERT(m_tUncompressedData == 0, "Uncompressed data already exists.");
		/*m_tUncompressedData = new VoxelType[m_uSideLength * m_uSideLength * m_uSideLength];

		VoxelType* pUncompressedData = m_tUncompressedData;		
		for(uint32_t ct = 0; ct < m_vecCompressedData.size(); ++ct)
		{
			std::fill(pUncompressedData, pUncompressedData + m_vecCompressedData[ct].length, m_vecCompressedData[ct].value);
			pUncompressedData += m_vecCompressedData[ct].length;
		}*/

		m_tUncompressedData = new VoxelType[m_uSideLength * m_uSideLength * m_uSideLength];

		void* pSrcData = reinterpret_cast<void*>(m_pCompressedData);
		void* pDstData = reinterpret_cast<void*>(m_tUncompressedData);
		uint32_t uSrcLength = m_uCompressedDataLength;
		uint32_t uDstLength = m_uSideLength * m_uSideLength * m_uSideLength * sizeof(VoxelType);

		MinizCompressor compressor;
		uint32_t uUncompressedLength = compressor.decompress(pSrcData, uSrcLength, pDstData, uDstLength);

		/*Data src;
		src.ptr = m_pCompressedData;
		src.length = m_uCompressedDataLength;

		Data dst;
		dst.ptr = reinterpret_cast<uint8_t*>(m_tUncompressedData);
		dst.length = m_uSideLength * m_uSideLength * m_uSideLength * sizeof(VoxelType);

		polyvox_decompress(src, dst);*/

		POLYVOX_ASSERT(uUncompressedLength == m_uSideLength * m_uSideLength * m_uSideLength * sizeof(VoxelType), "Destination length has changed.");

		//m_tUncompressedData = reinterpret_cast<VoxelType*>(uncompressedResult.ptr);

		m_bIsCompressed = false;
		m_bIsUncompressedDataModified = false;
	}
}
