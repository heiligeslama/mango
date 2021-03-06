/*
    MANGO Multimedia Development Platform
    Copyright (C) 2012-2018 Twilight Finland 3D Oy Ltd. All rights reserved.
*/
#include <cmath>
#include <cctype>
#include <mango/core/pointer.hpp>
#include <mango/core/buffer.hpp>
#include <mango/core/exception.hpp>
#include <mango/core/system.hpp>
#include <mango/image/image.hpp>

#define ID "ImageDecoder.PNM: "

namespace
{
    using namespace mango;

	const char* nextline(const char* p)
	{
		do
		{
			for ( ; *p++ != '\n'; )
			{
			}
		}
		while (*p == '#');
		return p;
	}

    // ------------------------------------------------------------
    // HeaderPNM
    // ------------------------------------------------------------

    struct HeaderPNM
    {
		int width;
		int height;
		int channels;
		int maxvalue;

		Format format;
		bool ascii;
        const char* data;

        HeaderPNM(Memory memory)
            : width(0)
            , height(0)
            , channels(0)
            , maxvalue(0)
            , ascii(false)
            , data(nullptr)
        {
            const char* p = reinterpret_cast<const char *>(memory.address);

            if (!std::strncmp(p, "P7\n", 3))
            {
                char type[100];

                p = nextline(p);
                if (std::sscanf(p, "WIDTH %i", &width) < 1)
                    MANGO_EXCEPTION(ID"Incorrect width");

                p = nextline(p);
                if (std::sscanf(p, "HEIGHT %i", &height) < 1)
                    MANGO_EXCEPTION(ID"Incorrect height");

                p = nextline(p);
                if (std::sscanf(p, "DEPTH %i", &channels) < 1)
                    MANGO_EXCEPTION(ID"Incorrect depth");

                p = nextline(p);
                if (std::sscanf(p, "MAXVAL %i", &maxvalue) < 1)
                    MANGO_EXCEPTION(ID"Incorrect maxval");

                p = nextline(p);
                if (std::sscanf(p, "TUPLTYPE %s", type) > 0)
                {
                    /*
                    if (!strncmp(type, "BLACKANDWHITE_ALPHA", strlen("BLACKANDWHITE_ALPHA")))
                    {
                        ++header.channels;
                    }
                    else if (!strncmp(type, "GRAYSCALE_ALPHA", strlen("GRAYSCALE_ALPHA")))
                    {
                        ++header.channels;
                    }
                    else if (!strncmp(type, "RGB_ALPHA", strlen("RGB_ALPHA")))
                    {
                        ++header.channels;
                    }
                    else
                    {
                        // custom type
                    }
                    */
                }

                p = nextline(p);
                if (std::strncmp(p, "ENDHDR\n", 7))
                    MANGO_EXCEPTION(ID"Incorrect endhdr");
            }
            else
            {
                if (!std::strncmp(p, "P1\n", 3))
                {
                    ascii = true;
                    channels = 1;
                    maxvalue = 1;
                }
                else if (!std::strncmp(p, "P2\n", 3))
                {
                    ascii = true;
                    channels = 1;
                }
                else if (!std::strncmp(p, "P3\n", 3))
                {
                    ascii = true;
                    channels = 3;
                }
                else if (!std::strncmp(p, "P4\n", 3))
                {
                    channels = 1;
                    maxvalue = 1;
                }
                else if (!std::strncmp(p, "P5\n", 3))
                {
                    channels = 1;
                }
                else if (!std::strncmp(p, "P6\n", 3))
                {
                    channels = 3;
                }
                else
                {
                    MANGO_EXCEPTION(ID"Incorrect header");
                }

                p = nextline(p);
                if (std::sscanf(p, "%i %i", &width, &height) < 2)
                    MANGO_EXCEPTION(ID"Incorrect header");

                if (!maxvalue)
                {
                    p = nextline(p);
                    if (std::sscanf(p, "%i", &maxvalue) < 1)
                        MANGO_EXCEPTION(ID"Incorrect header");
                }
            }

            if (maxvalue < 1 || maxvalue > 65535)
                MANGO_EXCEPTION(ID"Incorrect maxvalue");

            switch (channels)
            {
                case 1: format = Format(8, 0xff, 0); break;
                case 2: format = Format(16, 0x00ff, 0xff00); break;
                case 3: format = Format(24, Format::UNORM, Format::RGB, 8, 8, 8, 0); break;
                case 4: format = Format(32, Format::UNORM, Format::RGBA, 8, 8, 8, 8); break;
                default:
                    MANGO_EXCEPTION(ID"Incorrect number of channels");
            }

            data = p;
        }
    };

    // ------------------------------------------------------------
    // ImageDecoder
    // ------------------------------------------------------------

    struct Interface : ImageDecoderInterface
    {
        Memory m_memory;
        HeaderPNM m_header;

        Interface(Memory memory)
            : m_memory(memory)
            , m_header(memory)
        {
        }

        ~Interface()
        {
        }

        ImageHeader header() override
        {
            ImageHeader header;

            header.width   = m_header.width;
            header.height  = m_header.height;
            header.depth   = 0;
            header.levels  = 0;
            header.faces   = 0;
			header.palette = false;
            header.format  = m_header.format;
            header.compression = TextureCompression::NONE;

            return header;
        }

        void decode_matching(Surface& dest)
        {
            const char* p = m_header.data;
            const char* end = reinterpret_cast<const char *>(m_memory.address + m_memory.size);

    		p = nextline(p);
            const int xcount = m_header.width * m_header.channels;

            if (m_header.ascii)
            {
                for (int y = 0; y < m_header.height; ++y)
                {
                    u8* image = dest.address<u8>(0, y);

                    for (int x = 0; x < xcount; ++x)
                    {
                        int value = std::atoi(p);
                        image[x] = u8(value * 255 / m_header.maxvalue);

                        for ( ; !std::isspace(*p++); )
                        {
                        }

                        if (p >= end)
                            break;
                    }
                }
            }
            else
            {
                if (m_header.maxvalue <= 255)
                {
                    for (int y = 0; y < m_header.height; ++y)
                    {
                        u8* image = dest.address<u8>(0, y);
                        std::memcpy(image, p, xcount);
                        p += xcount;
                    }
                }
                else
                {
                    BigEndianPointer e = (u8*) p;

                    for (int y = 0; y < m_header.height; ++y)
                    {
                        u8* image = dest.address<u8>(0, y);

                        for (int x = 0; x < xcount; ++x)
                        {
                            int value = e.read16();
                            image[x] = u8(value * 255 / m_header.maxvalue);
                        }
                    }
                }
            }
        }

        void decode(Surface& dest, Palette* palette, int level, int depth, int face) override
        {
            MANGO_UNREFERENCED_PARAMETER(palette);
            MANGO_UNREFERENCED_PARAMETER(level);
            MANGO_UNREFERENCED_PARAMETER(depth);
            MANGO_UNREFERENCED_PARAMETER(face);

            if (dest.format == m_header.format &&
                dest.width >= m_header.width &&
                dest.height >= m_header.height)
            {
                decode_matching(dest);
            }
            else
            {
                Bitmap temp(m_header.width, m_header.height, m_header.format);
                decode_matching(temp);
                dest.blit(0, 0, temp);
            }
        }
    };

    ImageDecoderInterface* createInterface(Memory memory)
    {
        ImageDecoderInterface* x = new Interface(memory);
        return x;
    }

} // namespace

namespace mango
{

    void registerImageDecoderPNM()
    {
        registerImageDecoder(createInterface, "pbm");
        registerImageDecoder(createInterface, "pgm");
        registerImageDecoder(createInterface, "ppm");
        registerImageDecoder(createInterface, "pam");
    }

} // namespace mango
