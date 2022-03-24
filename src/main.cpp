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
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <spdlog/spdlog.h>

#include <string_view>

constexpr std::string_view DENSITY{ "@QB#NgWM8RDHdOKq9$6khEPXwmeZaoS2yjufF]}{tx1zv7lciL/\\|?*>r^;:_\"~,'.-`" };

constexpr const char* USAGE =
    R"(Image To Ascii
Usage:
    ascii [options] filename
Options:
        -a         Use fast perceived luminance algorithm
        -h, --help Show this message.
        -i         Invert brightness
        -n NUMBER  Number of spaces (' ') at the end of the density string. Default: 9
        -o FILE    Output path
        -p         Use perceived luminance
        -r RATIO   Font ratio for better sizing. RATIO is in the
                   format (FONT WIDTH:FONT HEIGHT) or (FONT WIDTH/FONT HEIGHT).
                   The default is 1:2.
)";

constexpr double RED_WEIGHT_PERC = 0.299;
constexpr double GREEN_WEIGHT_PERC = 0.587;
constexpr double BLUE_WEIGHT_PERC = 0.114;

constexpr double RED_WEIGHT = 0.2126;
constexpr double GREEN_WEIGHT = 0.7152;
constexpr double BLUE_WEIGHT = 0.0722;

constexpr double LUMA_MAX = 255;

constexpr const char RATIO_DELIM[3] = ":/";

struct Color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct Configuration
{
    bool print_usage = false;
    bool inverted = false;
    bool perceived = false;
    bool alt = false;

    uint32_t y_inc = 2;

    size_t num_spaces = 9;

    std::string_view input_path { };
    std::string_view output_path { };
};

constexpr uint32_t clamp(uint32_t x, uint32_t min, uint32_t max) {
    if(x < min) {
        return min;
    }

    if(x > max) {
        return max;
    }

    return x;
}

constexpr double luma(Color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT * red + GREEN_WEIGHT * green + BLUE_WEIGHT * blue) / LUMA_MAX;
}

constexpr double perceived_luma_fast(Color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT_PERC * red + GREEN_WEIGHT_PERC * green + BLUE_WEIGHT_PERC * blue) / LUMA_MAX;
}

constexpr double perceived_luma(Color pixel)
{
    auto red = static_cast<double>(pixel.red);
    auto green = static_cast<double>(pixel.green);
    auto blue = static_cast<double>(pixel.blue);

    return sqrt(RED_WEIGHT_PERC * red * red + GREEN_WEIGHT_PERC * green * green + BLUE_WEIGHT_PERC * blue * blue) / LUMA_MAX;
}

constexpr double average_luma(Configuration config, const std::unique_ptr<Color>& pixels, size_t width, size_t height, uint32_t x, uint32_t start_y)
{
    double numerator = 0;
    double denominator = 1;
    for (uint32_t y = start_y; y < height && static_cast<size_t>(y - start_y) < config.y_inc; y++) {
        Color pixel = pixels.get()[x + y * width];
        if (config.alt) {
            numerator += perceived_luma_fast(pixel);
        } else if (config.perceived) {
            numerator += perceived_luma(pixel);
        } else {
            numerator += luma(pixel);
        }

        if (y != start_y) {
            denominator++;
        }
    }

    return numerator / denominator;
}

void parse_arg(Configuration& config, const std::string_view& arg)
{
    static char previous_arg = '\0';

    if(!arg.starts_with('-')) {
        switch(previous_arg) {
            case 'n': config.num_spaces = std::stoull(arg.data()); break;
            case 'o': config.output_path = arg; break;
            case 'r': {
                std::unique_ptr<char> buffer {new char[arg.length()]};
                strcpy(buffer.get(), arg.data());
                const char* x_part = strtok(buffer.get(), RATIO_DELIM);
                const char* y_part = strtok(nullptr, RATIO_DELIM);

                if(y_part == nullptr) {
                    break;
                }

                for(const char* dummy = strtok(nullptr, RATIO_DELIM); dummy != nullptr; dummy = strtok(nullptr, RATIO_DELIM));
                config.y_inc = clamp(static_cast<uint32_t>(std::stof(y_part) / std::stof(x_part)), 1, 1000);
                break;
            }
            default: config.input_path = arg;
        }

        previous_arg = '\0';
        return;
    }

    if(arg.starts_with("--") && (strcmp("--help", arg.data()) == 0)) {
        config.print_usage = true;
        return;
    }

    for(size_t i = 1; i < arg.length(); i++) {
        char a = arg[i];

        if(previous_arg != '\0') {
            return parse_arg(config, arg.substr(i));
        }

        switch (a) {
            case 'a': config.alt = true; break;
            case 'h': config.print_usage = true; break;
            case 'i': config.inverted = true; break;
            case 'n': previous_arg = 'n'; break;
            case 'o': previous_arg = 'o'; break;
            case 'r': previous_arg = 'r'; break;
            case 'p': config.perceived = true; break;
            default: config.print_usage = true; break;
        }
    }
}

Configuration parse_command_line_args(int args, char* argv[])
{
    Configuration res;

    if(args < 2) {
        res.print_usage = true;
        return res;
    }

    for(int i = 1; i < args; i++) {
        spdlog::debug("argv[{}] = {}", i, argv[i]);
        parse_arg(res, argv[i]);
    }

    return res;
}

void doAsciiConversion(Configuration config, std::ostream& out, const std::unique_ptr<Color>& pixels, size_t width, size_t height) {
    for (uint32_t y = 0; y < height; y += config.y_inc) {
        for (uint32_t x = 0; x < width; x++) {
            double luminance = average_luma(config, pixels, width, height, x, y);

            if (!config.inverted) {
                luminance = (1 - luminance);
            }

            auto index = static_cast<size_t>(static_cast<double>(DENSITY.size() + config.num_spaces - 1) * luminance);

            if (index >= DENSITY.size()) {
                out << ' ';
            } else {
                out << DENSITY[index];
            }
        }

        out << '\n';
    }
}

int main(int args, char* argv[])
{
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#endif

    Configuration config = parse_command_line_args(args, argv);

    if (config.print_usage || config.input_path.empty()) {
        fmt::print(USAGE);
        return EXIT_SUCCESS;
    }

    int w = 0;
    int h = 0;
    int num_cmp = 0;
    uint8_t* comps = stbi_load(config.input_path.data(), &w, &h, &num_cmp, sizeof(Color));

    if (comps == nullptr || num_cmp != sizeof(Color)) {
        spdlog::critical("Failed to load {}", config.input_path);
        return EXIT_FAILURE;
    }

    auto width = static_cast<size_t>(w);
    auto height = static_cast<size_t>(h);

    size_t length = width * height;
    std::unique_ptr<Color> pixels{new Color[length] };
    memcpy(pixels.get(), comps, length * sizeof(Color));
    stbi_image_free(comps);

    if(config.output_path.empty()) {
        doAsciiConversion(config, std::cout, pixels, width, height);
        return EXIT_SUCCESS;
    }

    std::ofstream file(config.output_path.data());
    if (!file.is_open()) {
        spdlog::critical("Could not open {}", config.output_path);
        return EXIT_FAILURE;
    }

    doAsciiConversion(config, file, pixels, width, height);

    file.close();
    if (!file.good()) {
        spdlog::critical("Bad file: {}", config.output_path);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
