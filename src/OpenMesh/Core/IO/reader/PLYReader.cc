/* ========================================================================= *
 *                                                                           *
 *                               OpenMesh                                    *
 *           Copyright (c) 2001-2015, RWTH-Aachen University                 *
 *           Department for Computer Graphics and Multimedia                 *
 *                          All rights reserved.                             *
 *                            www.openmesh.org                               *
 *                                                                           *
 *---------------------------------------------------------------------------*
 * This file is part of OpenMesh.                                            *
 *---------------------------------------------------------------------------*
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 *                                                                           *
 * 1. Redistributions of source code must retain the above copyright notice, *
 *    this list of conditions and the following disclaimer.                  *
 *                                                                           *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 *    notice, this list of conditions and the following disclaimer in the    *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. Neither the name of the copyright holder nor the names of its          *
 *    contributors may be used to endorse or promote products derived from   *
 *    this software without specific prior written permission.               *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER *
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,  *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
 *                                                                           *
 * ========================================================================= */

/*===========================================================================*\
 *                                                                           *
 *   $Revision$                                                         *
 *   $Date$                   *
 *                                                                           *
 \*===========================================================================*/

#define LINE_LEN 4096

//== INCLUDES =================================================================

// OpenMesh
#include <OpenMesh/Core/System/config.h>
#include <OpenMesh/Core/System/omstream.hh>
#include <OpenMesh/Core/IO/reader/PLYReader.hh>
#include <OpenMesh/Core/IO/importer/BaseImporter.hh>
#include <OpenMesh/Core/IO/IOManager.hh>
#include <OpenMesh/Core/Utils/color_cast.hh>
#include <OpenMesh/Core/IO/SR_store.hh>

//STL
#include <fstream>
#include <memory>

#ifndef WIN32
#include <string.h>
#endif

//=== NAMESPACES ==============================================================


namespace OpenMesh {
namespace IO {

//=============================================================================

//=== INSTANCIATE =============================================================


_PLYReader_ __PLYReaderInstance;
_PLYReader_& PLYReader() {
    return __PLYReaderInstance;
}

//=== IMPLEMENTATION ==========================================================


_PLYReader_::_PLYReader_() {
    IOManager().register_module(this);

    // Store sizes in byte of each property type
    scalar_size_[ValueTypeINT8] = 1;
    scalar_size_[ValueTypeUINT8] = 1;
    scalar_size_[ValueTypeINT16] = 2;
    scalar_size_[ValueTypeUINT16] = 2;
    scalar_size_[ValueTypeINT32] = 4;
    scalar_size_[ValueTypeUINT32] = 4;
    scalar_size_[ValueTypeFLOAT32] = 4;
    scalar_size_[ValueTypeFLOAT64] = 8;

    scalar_size_[ValueTypeCHAR] = 1;
    scalar_size_[ValueTypeUCHAR] = 1;
    scalar_size_[ValueTypeSHORT] = 2;
    scalar_size_[ValueTypeUSHORT] = 2;
    scalar_size_[ValueTypeINT] = 4;
    scalar_size_[ValueTypeUINT] = 4;
    scalar_size_[ValueTypeFLOAT] = 4;
    scalar_size_[ValueTypeDOUBLE] = 8;
}

//-----------------------------------------------------------------------------


bool _PLYReader_::read(const std::string& _filename, BaseImporter& _bi, Options& _opt) {

    std::fstream in(_filename.c_str(), (std::ios_base::binary | std::ios_base::in) );

    if (!in.is_open() || !in.good()) {
        omerr() << "[PLYReader] : cannot not open file " << _filename << std::endl;
        return false;
    }

    bool result = read(in, _bi, _opt);

    in.close();
    return result;
}

//-----------------------------------------------------------------------------


bool _PLYReader_::read(std::istream& _in, BaseImporter& _bi, Options& _opt) {

    if (!_in.good()) {
        omerr() << "[PLYReader] : cannot not use stream" << std::endl;
        return false;
    }

    // filter relevant options for reading
    bool swap = _opt.check(Options::Swap);

    userOptions_ = _opt;

    // build options to be returned
    _opt.clear();

    if (options_.vertex_has_normal() && userOptions_.vertex_has_normal()) {
        _opt += Options::VertexNormal;
    }
    if (options_.vertex_has_texcoord() && userOptions_.vertex_has_texcoord()) {
        _opt += Options::VertexTexCoord;
    }
    if (options_.vertex_has_color() && userOptions_.vertex_has_color()) {
        _opt += Options::VertexColor;
    }
    if (options_.face_has_color() && userOptions_.face_has_color()) {
        _opt += Options::FaceColor;
    }
    if (options_.is_binary()) {
        _opt += Options::Binary;
    }
    if (options_.color_is_float()) {
        _opt += Options::ColorFloat;
    }
    if (options_.check(Options::Custom) && userOptions_.check(Options::Custom)) {
        _opt += Options::Custom;
    }

    //    //force user-choice for the alpha value when reading binary
    //    if ( options_.is_binary() && userOptions_.color_has_alpha() )
    //      options_ += Options::ColorAlpha;

    return (options_.is_binary() ? read_binary(_in, _bi, swap, _opt) : read_ascii(_in, _bi, _opt));

}

template<typename T>
void assignCustomProperty(std::istream& _in, BaseImporter& _bi, VertexHandle _vh, const std::string& _propName)
{
  OpenMesh::VPropHandleT<T> prop;
  if (!_bi.kernel()->get_property_handle(prop,_propName))
    _bi.kernel()->add_property(prop,_propName);
  T in;
  _in >> in;
  _bi.kernel()->property(prop,_vh) = in;
}


void _PLYReader_::readCustomProperty(std::istream& _in, BaseImporter& _bi, VertexHandle _vh, const std::string& _propName, const ValueType _valueType) const
{
  switch (_valueType)
  {
  case ValueTypeINT8:
  case ValueTypeCHAR:
      assignCustomProperty<char>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeINT16:
  case ValueTypeSHORT:
      assignCustomProperty<short>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeUINT16:
  case ValueTypeUSHORT:
      assignCustomProperty<unsigned short>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeINT32:
  case ValueTypeINT:
      assignCustomProperty<int>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeUINT32:
  case ValueTypeUINT:
      assignCustomProperty<unsigned int>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeFLOAT32:
  case ValueTypeFLOAT:
      assignCustomProperty<float>(_in,_bi,_vh,_propName);
      break;
  case ValueTypeFLOAT64:
  case ValueTypeDOUBLE:
      assignCustomProperty<double>(_in,_bi,_vh,_propName);
      break;
  default:
      std::cerr << "unsupported type" << std::endl;
      break;
  }
}


//-----------------------------------------------------------------------------

bool _PLYReader_::read_ascii(std::istream& _in, BaseImporter& _bi, const Options& _opt) const {

    // Reparse the header
    if (!can_u_read(_in)) {
        omerr() << "[PLYReader] : Unable to parse header\n";
        return false;
    }

    unsigned int i, j, k, l, idx;
    unsigned int nV;
    OpenMesh::Vec3f v, n;
    std::string trash;
    OpenMesh::Vec2f t;
    OpenMesh::Vec4i c;
    float tmp;
    BaseImporter::VHandles vhandles;
    VertexHandle vh;

    _bi.reserve(vertexCount_, 3* vertexCount_ , faceCount_);

    if (vertexDimension_ != 3) {
        omerr() << "[PLYReader] : Only vertex dimension 3 is supported." << std::endl;
        return false;
    }

    // read vertices:
    for (i = 0; i < vertexCount_ && !_in.eof(); ++i) {
        vh = _bi.add_vertex();

        v[0] = 0.0;
        v[1] = 0.0;
        v[2] = 0.0;

        n[0] = 0.0;
        n[1] = 0.0;
        n[2] = 0.0;

        t[0] = 0.0;
        t[1] = 0.0;

        c[0] = 0;
        c[1] = 0;
        c[2] = 0;
        c[3] = 255;

        for (uint propertyIndex = 0; propertyIndex < vertexPropertyCount_; ++propertyIndex) {
            switch (vertexPropertyMap_[propertyIndex].property) {
            case XCOORD:
                _in >> v[0];
                break;
            case YCOORD:
                _in >> v[1];
                break;
            case ZCOORD:
                _in >> v[2];
                break;
            case XNORM:
                _in >> n[0];
                break;
            case YNORM:
                _in >> n[1];
                break;
            case ZNORM:
                _in >> n[2];
                break;
            case TEXX:
                _in >> t[0];
                break;
            case TEXY:
                _in >> t[1];
                break;
            case COLORRED:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    _in >> tmp;
                    c[0] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    _in >> c[0];
                break;
            case COLORGREEN:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    _in >> tmp;
                    c[1] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    _in >> c[1];
                break;
            case COLORBLUE:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    _in >> tmp;
                    c[2] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    _in >> c[2];
                break;
            case COLORALPHA:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    _in >> tmp;
                    c[3] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    _in >> c[3];
                break;
            case CUSTOM_PROP:
                if (_opt.check(Options::Custom))
                  readCustomProperty(_in, _bi, vh, vertexPropertyMap_[propertyIndex].name, vertexPropertyMap_[propertyIndex].value);
                else
                  _in >> trash;
                break;
            default:
                _in >> trash;
                break;
            }
        }

        _bi.set_point(vh, v);
        if (_opt.vertex_has_normal())
          _bi.set_normal(vh, n);
        if (_opt.vertex_has_texcoord())
          _bi.set_texcoord(vh, t);
        if (_opt.vertex_has_color())
          _bi.set_color(vh, Vec4uc(c));
    }

    // faces
    // #N <v1> <v2> .. <v(n-1)> [color spec]
    for (i = 0; i < faceCount_; ++i) {
        // nV = number of Vertices for current face
        _in >> nV;

        if (nV == 3) {
            vhandles.resize(3);
            _in >> j;
            _in >> k;
            _in >> l;

            vhandles[0] = VertexHandle(j);
            vhandles[1] = VertexHandle(k);
            vhandles[2] = VertexHandle(l);
        } else {
            vhandles.clear();
            for (j = 0; j < nV; ++j) {
                _in >> idx;
                vhandles.push_back(VertexHandle(idx));
            }
        }

        _bi.add_face(vhandles);

    }

    // File was successfully parsed.
    return true;
}

//-----------------------------------------------------------------------------

bool _PLYReader_::read_binary(std::istream& _in, BaseImporter& _bi, bool /*_swap*/, const Options& _opt) const {

    // Reparse the header
    if (!can_u_read(_in)) {
        omerr() << "[PLYReader] : Unable to parse header\n";
        return false;
    }

    unsigned int i, j, k, l, idx;
    unsigned int nV;
    OpenMesh::Vec3f        v, n;  // Vertex
    OpenMesh::Vec2f        t;  // TexCoords
    BaseImporter::VHandles vhandles;
    VertexHandle           vh;
    OpenMesh::Vec4i        c;  // Color
    float                  tmp;

    _bi.reserve(vertexCount_, 3* vertexCount_ , faceCount_);

    // read vertices:
    for (i = 0; i < vertexCount_ && !_in.eof(); ++i) {
        v[0] = 0.0;
        v[1] = 0.0;
        v[2] = 0.0;

        n[0] = 0.0;
        n[1] = 0.0;
        n[2] = 0.0;

        t[0] = 0.0;
        t[1] = 0.0;

        c[0] = 0;
        c[1] = 0;
        c[2] = 0;
        c[3] = 255;

        for (uint propertyIndex = 0; propertyIndex < vertexPropertyCount_; ++propertyIndex) {
            switch (vertexPropertyMap_[propertyIndex].property) {
            case XCOORD:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, v[0]);
                break;
            case YCOORD:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, v[1]);
                break;
            case ZCOORD:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, v[2]);
                break;
            case XNORM:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, n[0]);
                break;
            case YNORM:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, n[1]);
                break;
            case ZNORM:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, n[2]);
                break;
            case TEXX:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, t[0]);
                break;
            case TEXY:
                readValue(vertexPropertyMap_[propertyIndex].value, _in, t[1]);
                break;
            case COLORRED:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    readValue(vertexPropertyMap_[propertyIndex].value, _in, tmp);

                    c[0] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    readInteger(vertexPropertyMap_[propertyIndex].value, _in, c[0]);

                break;
            case COLORGREEN:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    readValue(vertexPropertyMap_[propertyIndex].value, _in, tmp);
                    c[1] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    readInteger(vertexPropertyMap_[propertyIndex].value, _in, c[1]);

                break;
            case COLORBLUE:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    readValue(vertexPropertyMap_[propertyIndex].value, _in, tmp);
                    c[2] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    readInteger(vertexPropertyMap_[propertyIndex].value, _in, c[2]);

                break;
            case COLORALPHA:
                if (vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT32 ||
                    vertexPropertyMap_[propertyIndex].value == ValueTypeFLOAT) {
                    readValue(vertexPropertyMap_[propertyIndex].value, _in, tmp);
                    c[3] = static_cast<OpenMesh::Vec4i::value_type> (tmp * 255.0f);
                } else
                    readInteger(vertexPropertyMap_[propertyIndex].value, _in, c[3]);

                break;
            default:
                // Read unsupported property
                consume_input(_in, scalar_size_[vertexPropertyMap_[propertyIndex].value]);
                break;
            }

        }

        vh = _bi.add_vertex(v);
        if (_opt.vertex_has_normal())
          _bi.set_normal(vh, n);
        if (_opt.vertex_has_texcoord())
          _bi.set_texcoord(vh, t);
        if (_opt.vertex_has_color())
          _bi.set_color(vh, Vec4uc(c));
    }

    for (i = 0; i < faceCount_; ++i) {
        // Read number of vertices for the current face
        readValue(faceIndexType_, _in, nV);

        if (nV == 3) {
            vhandles.resize(3);
            readInteger(faceEntryType_, _in, j);
            readInteger(faceEntryType_, _in, k);
            readInteger(faceEntryType_, _in, l);

            vhandles[0] = VertexHandle(j);
            vhandles[1] = VertexHandle(k);
            vhandles[2] = VertexHandle(l);
        } else {
            vhandles.clear();
            for (j = 0; j < nV; ++j) {
                readInteger(faceEntryType_, _in, idx);
                vhandles.push_back(VertexHandle(idx));
            }
        }

        _bi.add_face(vhandles);
    }

    return true;
}


//-----------------------------------------------------------------------------


void _PLYReader_::readValue(ValueType _type, std::istream& _in, float& _value) const {

    switch (_type) {
    case ValueTypeFLOAT32:
    case ValueTypeFLOAT:
        float32_t tmp;
        restore(_in, tmp, options_.check(Options::MSB));
        _value = tmp;
        break;
    default:
        _value = 0.0;
        std::cerr << "unsupported conversion type to float: " << _type << std::endl;
        break;
    }
}


//-----------------------------------------------------------------------------


void _PLYReader_::readValue(ValueType _type, std::istream& _in, double& _value) const {

    switch (_type) {

        case ValueTypeFLOAT64:

        case ValueTypeDOUBLE:

            float64_t tmp;
            restore(_in, tmp, options_.check(Options::MSB));
            _value = tmp;

            break;

    default:

        _value = 0.0;
        std::cerr << "unsupported conversion type to double: " << _type << std::endl;

        break;
    }
}


//-----------------------------------------------------------------------------


void _PLYReader_::readValue(ValueType _type, std::istream& _in, unsigned int& _value) const {

    uint32_t tmp_uint32_t;
    uint16_t tmp_uint16_t;
    uint8_t tmp_uchar;

    switch (_type) {

        case ValueTypeUINT:

        case ValueTypeUINT32:

            restore(_in, tmp_uint32_t, options_.check(Options::MSB));
            _value = tmp_uint32_t;

        break;

        case ValueTypeUSHORT:

        case ValueTypeUINT16:

            restore(_in, tmp_uint16_t, options_.check(Options::MSB));
            _value = tmp_uint16_t;

            break;

        case ValueTypeUCHAR:

        case ValueTypeUINT8:

            restore(_in, tmp_uchar, options_.check(Options::MSB));
            _value = tmp_uchar;

            break;

        default:

            _value = 0;
            std::cerr << "unsupported conversion type to unsigned int: " << _type << std::endl;

            break;
    }
}


//-----------------------------------------------------------------------------


void _PLYReader_::readValue(ValueType _type, std::istream& _in, int& _value) const {

    int32_t tmp_int32_t;
    int16_t tmp_int16_t;
    int8_t tmp_char;

    switch (_type) {

        case ValueTypeINT:

        case ValueTypeINT32:

            restore(_in, tmp_int32_t, options_.check(Options::MSB));
            _value = tmp_int32_t;

            break;

        case ValueTypeSHORT:

        case ValueTypeINT16:

            restore(_in, tmp_int16_t, options_.check(Options::MSB));
            _value = tmp_int16_t;

            break;

        case ValueTypeCHAR:

        case ValueTypeINT8:

            restore(_in, tmp_char, options_.check(Options::MSB));
            _value = tmp_char;

            break;

        default:

            _value = 0;
            std::cerr << "unsupported conversion type to int: " << _type << std::endl;

            break;
    }
}


//-----------------------------------------------------------------------------


void _PLYReader_::readInteger(ValueType _type, std::istream& _in, int& _value) const {

    int32_t tmp_int32_t;
    uint32_t tmp_uint32_t;
    int8_t tmp_char;
    uint8_t tmp_uchar;

    switch (_type) {

        case ValueTypeINT:

        case ValueTypeINT32:

            restore(_in, tmp_int32_t, options_.check(Options::MSB));
            _value = tmp_int32_t;

            break;

        case ValueTypeUINT:

        case ValueTypeUINT32:

            restore(_in, tmp_uint32_t, options_.check(Options::MSB));
            _value = tmp_uint32_t;

            break;

        case ValueTypeCHAR:

        case ValueTypeINT8:

            restore(_in, tmp_char, options_.check(Options::MSB));
            _value = tmp_char;

            break;

        case ValueTypeUCHAR:

        case ValueTypeUINT8:

            restore(_in, tmp_uchar, options_.check(Options::MSB));
            _value = tmp_uchar;

            break;

        default:

            _value = 0;
            std::cerr << "unsupported conversion type to int: " << _type << std::endl;

            break;
    }
}


//-----------------------------------------------------------------------------


void _PLYReader_::readInteger(ValueType _type, std::istream& _in, unsigned int& _value) const {

    int32_t tmp_int32_t;
    uint32_t tmp_uint32_t;
    int8_t tmp_char;
    uint8_t tmp_uchar;

    switch (_type) {

        case ValueTypeUINT:

        case ValueTypeUINT32:

            restore(_in, tmp_uint32_t, options_.check(Options::MSB));
            _value = tmp_uint32_t;

            break;

        case ValueTypeINT:

        case ValueTypeINT32:

            restore(_in, tmp_int32_t, options_.check(Options::MSB));
            _value = tmp_int32_t;

            break;

        case ValueTypeUCHAR:

        case ValueTypeUINT8:

            restore(_in, tmp_uchar, options_.check(Options::MSB));
            _value = tmp_uchar;

            break;

        case ValueTypeCHAR:

        case ValueTypeINT8:

            restore(_in, tmp_char, options_.check(Options::MSB));
            _value = tmp_char;

            break;

        default:

            _value = 0;
            std::cerr << "unsupported conversion type to unsigned int: " << _type << std::endl;

            break;
    }
}


//------------------------------------------------------------------------------


bool _PLYReader_::can_u_read(const std::string& _filename) const {

    // !!! Assuming BaseReader::can_u_parse( std::string& )
    // does not call BaseReader::read_magic()!!!

    if (BaseReader::can_u_read(_filename)) {
        std::ifstream ifs(_filename.c_str());
        if (ifs.is_open() && can_u_read(ifs)) {
            ifs.close();
            return true;
        }
    }
    return false;
}



//-----------------------------------------------------------------------------

std::string get_property_name(std::string _string1, std::string _string2) {

    if (_string1 == "float32" || _string1 == "float64" || _string1 == "float" || _string1 == "double" ||
        _string1 == "int8" || _string1 == "uint8" || _string1 == "char" || _string1 == "uchar" ||
        _string1 == "int32" || _string1 == "uint32" || _string1 == "int" || _string1 == "uint" ||
        _string1 == "int16" || _string1 == "uint16" || _string1 == "short" || _string1 == "ushort")
        return _string2;

    if (_string2 == "float32" || _string2 == "float64" || _string2 == "float" || _string2 == "double" ||
        _string2 == "int8" || _string2 == "uint8" || _string2 == "char" || _string2 == "uchar" ||
        _string2 == "int32" || _string2 == "uint32" || _string2 == "int" || _string2 == "uint" ||
        _string2 == "int16" || _string2 == "uint16" || _string2 == "short" || _string2 == "ushort")
        return _string1;


    std::cerr << "Unsupported entry type" << std::endl;
    return "Unsupported";
}

//-----------------------------------------------------------------------------

_PLYReader_::ValueType get_property_type(std::string _string1, std::string _string2) {

    if (_string1 == "float32" || _string2 == "float32")

        return _PLYReader_::ValueTypeFLOAT32;

    else if (_string1 == "float64" || _string2 == "float64")

        return _PLYReader_::ValueTypeFLOAT64;

    else if (_string1 == "float" || _string2 == "float")

        return _PLYReader_::ValueTypeFLOAT;

    else if (_string1 == "double" || _string2 == "double")

        return _PLYReader_::ValueTypeDOUBLE;

    else if (_string1 == "int8" || _string2 == "int8")

        return _PLYReader_::ValueTypeINT8;

    else if (_string1 == "uint8" || _string2 == "uint8")

        return _PLYReader_::ValueTypeUINT8;

    else if (_string1 == "char" || _string2 == "char")

        return _PLYReader_::ValueTypeCHAR;

    else if (_string1 == "uchar" || _string2 == "uchar")

        return _PLYReader_::ValueTypeUCHAR;

    else if (_string1 == "int32" || _string2 == "int32")

        return _PLYReader_::ValueTypeINT32;

    else if (_string1 == "uint32" || _string2 == "uint32")

        return _PLYReader_::ValueTypeUINT32;

    else if (_string1 == "int" || _string2 == "int")

        return _PLYReader_::ValueTypeINT;

    else if (_string1 == "uint" || _string2 == "uint")

        return _PLYReader_::ValueTypeUINT;

    else if (_string1 == "int16" || _string2 == "int16")

        return _PLYReader_::ValueTypeINT16;

    else if (_string1 == "uint16" || _string2 == "uint16")

        return _PLYReader_::ValueTypeUINT16;

    else if (_string1 == "short" || _string2 == "short")

        return _PLYReader_::ValueTypeSHORT;

    else if (_string1 == "ushort" || _string2 == "ushort")

        return _PLYReader_::ValueTypeUSHORT;

    return _PLYReader_::Unsupported;
}


//-----------------------------------------------------------------------------

bool _PLYReader_::can_u_read(std::istream& _is) const {

    // Clear per file options
    options_.cleanup();

    // clear vertex property map, will be recreated
    vertexPropertyMap_.clear();
    vertexPropertyCount_ = 0;

    // read 1st line
    std::string line;
    std::getline(_is, line);
    trim(line);

    //Check if this file is really a ply format
    if (line != "PLY" && line != "ply")
        return false;

    vertexCount_ = 0;
    faceCount_ = 0;
    vertexDimension_ = 0;

    std::string keyword;
    std::string fileType;
    std::string elementName = "";
    std::string propertyName;
    std::string listIndexType;
    std::string listEntryType;
    float version;

    _is >> keyword;
    _is >> fileType;
    _is >> version;

    if (_is.bad()) {
        omerr() << "Defect PLY header detected" << std::endl;
        return false;
    }

    if (fileType == "ascii") {
        options_ -= Options::Binary;
    } else if (fileType == "binary_little_endian") {
        options_ += Options::Binary;
        options_ += Options::LSB;
        //if (Endian::local() == Endian::MSB)

        //  options_ += Options::Swap;
    } else if (fileType == "binary_big_endian") {
        options_ += Options::Binary;
        options_ += Options::MSB;
        //if (Endian::local() == Endian::LSB)

        //  options_ += Options::Swap;
    } else {
        omerr() << "Unsupported PLY format: " << fileType << std::endl;
        return false;
    }

    std::streamoff streamPos = _is.tellg();
    _is >> keyword;
    while (keyword != "end_header") {

        if (keyword == "comment") {
            std::getline(_is, line);
        } else if (keyword == "element") {
            _is >> elementName;
            if (elementName == "vertex") {
                _is >> vertexCount_;
            } else if (elementName == "face") {
                _is >> faceCount_;
            } else {
                omerr() << "PLY header unsupported element type: " << elementName << std::endl;
            }
        } else if (keyword == "property") {
            std::string tmp1;
            std::string tmp2;

            // Read first keyword, as it might be a list
            _is >> tmp1;

            if (tmp1 == "list") {
                if (elementName == "vertex") {
                    omerr() << "List type not supported for vertices!" << std::endl;
                } else if (elementName == "face") {
                    _is >> listIndexType;
                    _is >> listEntryType;
                    _is >> propertyName;

                    if (listIndexType == "uint8") {
                        faceIndexType_ = ValueTypeUINT8;
                    } else if (listIndexType == "uchar") {
                        faceIndexType_ = ValueTypeUCHAR;
                    } else {
                        omerr() << "Unsupported Index type for face list: " << listIndexType << std::endl;
                        return false;
                    }

                    if (listEntryType == "int32") {
                        faceEntryType_ = ValueTypeINT32;
                    } else if (listEntryType == "int") {
                        faceEntryType_ = ValueTypeINT;
                    } else if (listEntryType == "uint32") {
                        faceEntryType_ = ValueTypeUINT32;
                    } else if (listEntryType == "uint") {
                        faceEntryType_ = ValueTypeUINT;
                    } else {
                        omerr() << "Unsupported Entry type for face list: " << listEntryType << std::endl;
                        return false;
                    }

                }
            } else {
                // as this is not a list property, read second value of property
                _is >> tmp2;

                if (elementName == "vertex") {
                    // Extract name and type of property
                    // As the order seems to be different in some files, autodetect it.
                    ValueType valueType = get_property_type(tmp1, tmp2);
                    propertyName = get_property_name(tmp1, tmp2);

                    if (propertyName == "x") {
                      VertexPropertyInfo entry(XCOORD, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      vertexDimension_++;
                    } else if (propertyName == "y") {
                      VertexPropertyInfo entry(YCOORD, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      vertexDimension_++;
                    } else if (propertyName == "z") {
                      VertexPropertyInfo entry(ZCOORD, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      vertexDimension_++;
                    } else if (propertyName == "nx") {
                      VertexPropertyInfo entry(XNORM, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexNormal;
                    } else if (propertyName == "ny") {
                      VertexPropertyInfo entry(YNORM, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexNormal;
                    } else if (propertyName == "nz") {
                      VertexPropertyInfo entry(ZNORM, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexNormal;
                    } else if (propertyName == "u" || propertyName == "s") {
                      VertexPropertyInfo entry(TEXX, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexTexCoord;
                    } else if (propertyName == "v" || propertyName == "t") {
                      VertexPropertyInfo entry(TEXY, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexTexCoord;
                    } else if (propertyName == "red") {
                      VertexPropertyInfo entry(COLORRED, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "green") {
                      VertexPropertyInfo entry(COLORGREEN, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "blue") {
                      VertexPropertyInfo entry(COLORBLUE, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "diffuse_red") {
                      VertexPropertyInfo entry(COLORRED, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "diffuse_green") {
                      VertexPropertyInfo entry(COLORGREEN, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "diffuse_blue") {
                      VertexPropertyInfo entry(COLORBLUE, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else if (propertyName == "alpha") {
                      VertexPropertyInfo entry(COLORALPHA, valueType);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                      options_ += Options::VertexColor;
                      options_ += Options::ColorAlpha;
                      if (valueType == ValueTypeFLOAT || valueType == ValueTypeFLOAT32)
                        options_ += Options::ColorFloat;
                    } else {
                      VertexProperty prop = (!options_.is_binary()) ? CUSTOM_PROP : UNSUPPORTED; // loading vertex properties is not yet supported by the binary loader
                      if (prop != UNSUPPORTED)
                        options_ += Options::Custom;
                      VertexPropertyInfo entry(prop, valueType, propertyName);
                      vertexPropertyMap_[vertexPropertyCount_] = entry;
                    }

                    vertexPropertyCount_++;

                } else if (elementName == "face") {
                    omerr() << "Properties not supported for faces " << std::endl;
                }

            }

        } else {
            omlog() << "Unsupported keyword : " << keyword << std::endl;
        }

        streamPos = _is.tellg();
        _is >> keyword;
        if (_is.bad()) {
            omerr() << "Error while reading PLY file header" << std::endl;
            return false;
        }
    }

    // As the binary data is directy after the end_header keyword
    // and the stream removes too many bytes, seek back to the right position
    if (options_.is_binary()) {
        _is.seekg(streamPos + 12);
    }

    return true;
}

//=============================================================================
} // namespace IO
} // namespace OpenMesh
//=============================================================================
