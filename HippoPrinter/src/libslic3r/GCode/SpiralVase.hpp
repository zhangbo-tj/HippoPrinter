#ifndef slic3r_SpiralVase_hpp_
#define slic3r_SpiralVase_hpp_


#include <src/libslic3r/libslic3r.h>
#include <src/libslic3r/GCode.hpp>
#include <src/libslic3r/GCodeReader.hpp>

namespace Slic3r {

class SpiralVase {
    public:
    bool enable;
    
    SpiralVase(const PrintConfig &config)
        : enable(false), _config(&config)
    {
        this->_reader.Z = this->_config->z_offset;
        this->_reader.apply_config(*this->_config);
    };
    std::string process_layer(const std::string &gcode);
    
    private:
    const PrintConfig* _config;
    GCodeReader _reader;
};

}

#endif
