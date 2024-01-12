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

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wold-style-cast"
#   pragma clang diagnostic ignored "-Wsign-conversion"
#   pragma clang diagnostic ignored "-Wcast-align"
#   pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#   pragma clang diagnostic ignored "-Wdouble-promotion"
#elif defined(__GNUC__) || defined(__GNUG__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wold-style-cast"
#   pragma GCC diagnostic ignored "-Wsign-conversion"
#   pragma GCC diagnostic ignored "-Wcast-align"
#   pragma GCC diagnostic ignored "-Wdouble-promotion"
#   pragma GCC diagnostic ignored "-Wduplicated-branches"
#   pragma GCC diagnostic ignored "-Wuseless-cast"
#   pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <stb_image.h>

#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#   pragma GCC diagnostic pop
#endif

#include <string_view>

static constexpr std::string_view DENSITY{ "@QB#NgWM8RDHdOKq9$6khEPXwmeZaoS2yjufF]}{tx1zv7lciL/\\|?*>r^;:_\"~,'.-`" };

static constexpr const char* USAGE =
    R"(Image To Ascii
Usage:
    ascii [options] filename
Options:
        -W COLUMNS Set number of columns for output, 
                   rows will be calculated from aspect ratio if not provided.
        -H ROWS    Set number of rows for output, 
                   columns will be calculated from aspect ratio if not provided.

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

static constexpr double RED_WEIGHT_PERC = 0.299;
static constexpr double GREEN_WEIGHT_PERC = 0.587;
static constexpr double BLUE_WEIGHT_PERC = 0.114;

static constexpr double RED_WEIGHT = 0.2126;
static constexpr double GREEN_WEIGHT = 0.7152;
static constexpr double BLUE_WEIGHT = 0.0722;

static constexpr double LUMA_MAX = 255;

static constexpr char RATIO_DELIM[3] = ":/";

struct Color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct Quad
{
    double top_left_x;
    double top_left_y;
    double width;
    double height;
};

struct Configuration
{
    bool print_usage = false;
    bool inverted = false;
    bool perceived = false;
    bool alt = false;

    uint32_t cols = -1U;
    uint32_t rows = -1U;

    double font_ratio = 0.5;

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

constexpr double luma(const Color& pixel)
{
    const auto red = static_cast<double>(pixel.red);
    const auto green = static_cast<double>(pixel.green);
    const auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT * red + GREEN_WEIGHT * green + BLUE_WEIGHT * blue) / LUMA_MAX;
}

constexpr double perceived_luma_fast(const Color& pixel)
{
    const auto red = static_cast<double>(pixel.red);
    const auto green = static_cast<double>(pixel.green);
    const auto blue = static_cast<double>(pixel.blue);

    return (RED_WEIGHT_PERC * red + GREEN_WEIGHT_PERC * green + BLUE_WEIGHT_PERC * blue) / LUMA_MAX;
}

constexpr double perceived_luma(const Color& pixel)
{
    const auto red = static_cast<double>(pixel.red);
    const auto green = static_cast<double>(pixel.green);
    const auto blue = static_cast<double>(pixel.blue);

    return sqrt(RED_WEIGHT_PERC * red * red + GREEN_WEIGHT_PERC * green * green + BLUE_WEIGHT_PERC * blue * blue) / LUMA_MAX;
}

constexpr double average_luma(const Configuration& config, const std::unique_ptr<Color>& pixels, const Quad& region, size_t img_width, size_t img_height)
{
    double luma_accumulator = 0;
    double pixel_count = 0;

    for(size_t y = static_cast<size_t>(region.top_left_y); y < std::min(img_height, static_cast<size_t>(region.top_left_y + region.height)); y++) {
        for(size_t x = static_cast<size_t>(region.top_left_x); x < std::min(img_width, static_cast<size_t>(region.top_left_x + region.width)); x++) {
            Color pixel = pixels.get()[x + y * img_width];
            if (config.alt) {
                luma_accumulator += perceived_luma_fast(pixel);
            } else if (config.perceived) {
                luma_accumulator += perceived_luma(pixel);
            } else {
                luma_accumulator += luma(pixel);
            }

            pixel_count++;
        }
    }

    if(pixel_count == 0) {
        return luma_accumulator;
    } else {
        return luma_accumulator / pixel_count;
    }
}

void doAsciiConversion(const Configuration& config, std::ostream& out, const std::unique_ptr<Color>& pixels, size_t img_width, size_t img_height) {
    double quad_width = static_cast<double>(img_width) / static_cast<double>(config.cols);
    double quad_height = static_cast<double>(img_height) / (static_cast<double>(config.rows) * config.font_ratio);

    for (double y = 0; y < static_cast<double>(img_height); y += quad_height) {
        for (double x = 0; x < static_cast<double>(img_width); x += quad_width) {
            Quad char_quad { x, y, quad_width, quad_height };
            double luminance = average_luma(config, pixels, char_quad, img_width, img_height);

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

void parse_arg(Configuration& config, const std::string_view& arg)
{
    static char previous_arg = '\0';

    if(!arg.starts_with('-')) {
        switch(previous_arg) {
            case 'W': config.cols = static_cast<uint32_t>(std::stoi(arg.data())); break;
            case 'H': config.rows = static_cast<uint32_t>(std::stoi(arg.data())); break;
            case 'n': config.num_spaces = std::stoull(arg.data()); break;
            case 'o': config.output_path = arg; break;
            case 'r': {
                const auto buffer { std::make_unique<char>(arg.length()) };
                strcpy(buffer.get(), arg.data());
                const char* x_part = strtok(buffer.get(), RATIO_DELIM);
                const char* y_part = strtok(nullptr, RATIO_DELIM);

                if(y_part == nullptr) {
                    break;
                }

                for(const char* dummy = strtok(nullptr, RATIO_DELIM); dummy != nullptr; dummy = strtok(nullptr, RATIO_DELIM));
                config.font_ratio = std::stod(x_part) / std::stod(y_part);
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
            case 'W':
            case 'H':         
            case 'n':
            case 'o':
            case 'r': previous_arg = a; break;

            case 'a': config.alt = true; break;
            case 'h': config.print_usage = true; break;
            case 'i': config.inverted = true; break;
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
#ifndef NDEBUG
        std::cout << "argv[" << i << "] = " << argv[i] << '\n';
#endif
        parse_arg(res, argv[i]);
    }

    return res;
}

int main(int args, char* argv[])
{
    Configuration config = parse_command_line_args(args, argv);

    if (config.print_usage || config.input_path.empty()) {
        std::cout << USAGE;
        return EXIT_SUCCESS;
    }

    int w, h, n;
    uint8_t* comps = stbi_load(config.input_path.data(), &w, &h, &n, sizeof(Color));

    if (comps == nullptr) {
        std::cerr << "Failed to load " << config.input_path << '\n';
        return EXIT_FAILURE;
    }

    auto width = static_cast<size_t>(w);
    auto height = static_cast<size_t>(h);

    size_t length = width * height;
    auto pixels { std::make_unique<Color>(length) };
    memcpy(pixels.get(), comps, length * sizeof(Color));
    stbi_image_free(comps);

    //columns and rows normalization
    if(config.cols == -1U && config.rows == -1U) {
        config.cols = static_cast<uint32_t>(width);
        config.rows = static_cast<uint32_t>(height);
    } else if(config.cols == -1U) {
        double cols = static_cast<double>(config.rows) * static_cast<double>(width) / static_cast<double>(height);
        config.cols = static_cast<uint32_t>(cols + 1); //Use Ceiling to cover leftover image
    } else if(config.rows == -1U) {
        double rows = static_cast<double>(config.cols) * static_cast<double>(height) / static_cast<double>(width);
        config.rows = static_cast<uint32_t>(rows + 1); //Use Ceiling to cover leftover image
    }

    if(config.output_path.empty()) {
        doAsciiConversion(config, std::cout, pixels, width, height);
        return EXIT_SUCCESS;
    }

    std::ofstream file(config.output_path.data());
    if (!file.is_open()) {
        std::cerr << "Could not open " << config.output_path << '\n';
        return EXIT_FAILURE;
    }

    doAsciiConversion(config, file, pixels, width, height);

    file.close();
    if (!file.good()) {
        std::cerr << "Bad file: " << config.output_path << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
