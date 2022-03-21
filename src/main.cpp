/*
    Copyright 2022 Eduardo Ibarra

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include <memory>

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <spdlog/spdlog.h>

#include <string_view>

constexpr std::string_view DENSITY{ "@QB#NgWM8RDHdOKq9$6khEPXwmeZaoS2yjufF]}{tx1zv7lciL/\\|?*>r^;:_\"~,'.-`" };

constexpr const char* OUT_PATH = "./out.txt";
constexpr const char* USAGE =
    R"(Image To Ascii
Usage:
    ascii [options] filename
Options:
        -a         Use fast perceived luminance algorithm
        -h, --help Show this message.
        -i         Invert brightness
        -n NUMBER  Number of ' ' at the end of the density string. Default: 9
        -o FILE    Output path
        -p         Use perceived luminance
)";

constexpr double RED_WEIGHT_PERC = 0.299;
constexpr double GREEN_WEIGHT_PERC = 0.587;
constexpr double BLUE_WEIGHT_PERC = 0.114;

constexpr double RED_WEIGHT = 0.2126;
constexpr double GREEN_WEIGHT = 0.7152;
constexpr double BLUE_WEIGHT = 0.0722;

constexpr double LUMA_MAX = 255;

struct color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct configuration
{
    bool malformed = false;
    bool inverted = false;
    bool perceived = false;
    bool alt = false;

    // TODO: Needs better name
    size_t x_thingy = 1;
    size_t y_thingy = 2;

    size_t num_spaces = 9;

    const char* output_path = OUT_PATH;
} __attribute__((packed));

constexpr double luma(color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT * red + GREEN_WEIGHT * green + BLUE_WEIGHT * blue) / LUMA_MAX;
}

constexpr double perceived_luma_fast(color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT_PERC * red + GREEN_WEIGHT_PERC * green + BLUE_WEIGHT_PERC * blue) / LUMA_MAX;
}

constexpr double perceived_luma(color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return sqrt(RED_WEIGHT_PERC * red * red + GREEN_WEIGHT_PERC * green * green + BLUE_WEIGHT_PERC * blue * blue) / LUMA_MAX;
}

constexpr double average_luma(configuration config, const std::unique_ptr<color>& pixels, int width, int height, int startx, int starty)
{
    double luminance = 0;
    double denom = 1;
    for (int y = starty; y < height && static_cast<size_t>(y - starty) < config.y_thingy; y++) {
        for (int x = startx; x < width && static_cast<size_t>(x - startx) < config.x_thingy; x++) {
            color pixel = pixels.get()[x + y * width];
            if (config.alt) {
                luminance += perceived_luma_fast(pixel);
            } else if (config.perceived) {
                luminance += perceived_luma(pixel);
            } else {
                luminance += luma(pixel);
            }

            if (y != starty || x != startx) {
                denom++;
            }
        }
    }

    luminance /= denom;
    return luminance;
}

configuration parse_command_line_args(int args, char* argv[])
{
    configuration res;

    if (args < 2) {
        fmt::print(USAGE);
        res.malformed = true;
        return res;
    }

    for (int i = 0; i < args; i++) {
        spdlog::debug("argv[{}] = {}", i, argv[i]);
        if (argv[i][0] != '-') {
            continue;
        }

        size_t length = strlen(argv[i]);
        for (size_t charIndex = 1; charIndex < length; charIndex++) {
            switch (argv[i][charIndex]) {
            case '-': {
                if (charIndex + 1 < length && strcmp(&argv[i][charIndex + 1], "help")) {
                    fmt::print(USAGE);
                    res.malformed = true;
                    return res;
                }
                break;
            }
            case 'a':
                res.alt = true;
                break;
            case 'h': {
                fmt::print(USAGE);
                res.malformed = true;
                return res;
            }
            case 'i':
                res.inverted = true;
                break;
            case 'n': {
                if (charIndex + 1 >= length) {
                    res.num_spaces = std::stoull(argv[++i]);
                } else {
                    res.num_spaces = std::stoull(&argv[i][charIndex + 1]);
                }
                break;
            }

            case 'o': {
                if (charIndex + 1 >= length) {
                    res.output_path = argv[++i];
                } else {
                    res.output_path = &argv[i][charIndex + 1];
                }
                break;
            }
            case 'p':
                res.perceived = true;
                break;
            default:
                continue;
            }
        }
    }

    return res;
}

int main(int args, char* argv[])
{
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    configuration config = parse_command_line_args(args, argv);

    if (config.malformed) {
        return EXIT_FAILURE;
    }

    const char* img_path = argv[args - 1];
    int width = 0;
    int height = 0;
    int num_cmp = 0;
    uint8_t* comps = stbi_load(img_path, &width, &height, &num_cmp, sizeof(color));

    if (comps == nullptr || num_cmp != sizeof(color)) {
        spdlog::critical("Failed to load {}", img_path);
        return EXIT_FAILURE;
    }

    size_t length = static_cast<size_t>(width * height);
    std::unique_ptr<color> pixels{ new color[length] };
    memcpy(pixels.get(), comps, length * sizeof(color));
    stbi_image_free(comps);

    std::ofstream file(config.output_path);
    if (!file.is_open()) {
        spdlog::critical("Could not open {}", config.output_path);
        return EXIT_FAILURE;
    }

    for (int y = 0; y < height; y += config.y_thingy) {
        for (int x = 0; x < width; x += config.x_thingy) {
            double luminance = average_luma(config, pixels, width, height, x, y);

            if (!config.inverted) {
                luminance = (1 - luminance);
            }

            auto index = static_cast<size_t>(static_cast<double>(DENSITY.size() + config.num_spaces) * luminance);

            if (index >= DENSITY.size()) {
                file << ' ';
            } else {
                file << DENSITY[index];
            }
        }
        file << '\n';
    }

    file.close();
    if (!file.good()) {
        spdlog::critical("Bad file: {}", config.output_path);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
