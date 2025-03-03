/*
 * libcdoc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

%module(directors="1") CDoc

%{
#include "CDoc.h"
#include "Io.h"
#include "Configuration.h"
#include "CDocWriter.h"
#include "CDocReader.h"
#include "Lock.h"
#include "NetworkBackend.h"
#include "PKCS11Backend.h"
#include "Recipient.h"
#include "Utils.h"
#include "Wrapper.h"
#include <iostream>

#ifdef SWIGCSHARP
extern "C"
{
    SWIGEXPORT unsigned char* SWIGSTDCALL ByteVectorData(void* ptr) {
        return static_cast<std::vector<unsigned char>*>(ptr)->data();
    }
    SWIGEXPORT int SWIGSTDCALL ByteVectorSize(void* ptr) {
       return static_cast<std::vector<unsigned char>*>(ptr)->size();
    }
    SWIGEXPORT void SWIGSTDCALL ByteVectorFree(void* ptr) {
        delete static_cast<std::vector<unsigned char>*>(ptr);
    }
    SWIGEXPORT void* SWIGSTDCALL ByteVectorTo(unsigned char* data, int size) {
       return new std::vector<unsigned char>(data, data + size);
    }
}
#endif
%}

%apply unsigned char { uint8_t }
%apply int { int32_t }
%apply unsigned int { uint32_t }
%apply unsigned long long { uint64_t }
%apply long long { int64_t }
%apply long long { libcdoc::result_t }

%pragma(csharp) imclasscode=%{
  [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="ByteVectorData")]
  public static extern global::System.IntPtr ByteVectorData(global::System.IntPtr data);
  [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="ByteVectorSize")]
  public static extern int ByteVectorSize(global::System.IntPtr data);
  [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="ByteVectorFree")]
  public static extern void ByteVectorFree(global::System.IntPtr data);
  [global::System.Runtime.InteropServices.DllImport("$dllimport", EntryPoint="ByteVectorTo")]
  public static extern global::System.IntPtr ByteVectorTo(
  [global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPArray)]byte[] data, int size);
%}

%typemap(cstype) std::vector<uint8_t> "byte[]"
%typemap(csin, pre= "    global::System.IntPtr cPtr$csinput = CDocPINVOKE.ByteVectorTo($csinput, $csinput.Length);
    var handleRef$csinput = new global::System.Runtime.InteropServices.HandleRef(this, cPtr$csinput);"
) std::vector<uint8_t> "handleRef$csinput"
%typemap(csout, excode=SWIGEXCODE) std::vector<uint8_t> {
    global::System.IntPtr cPtr = $imcall;$excode
    byte[] result = new byte[$modulePINVOKE.ByteVectorSize(cPtr)];
    global::System.Runtime.InteropServices.Marshal.Copy($modulePINVOKE.ByteVectorData(cPtr), result, 0, result.Length);
    $modulePINVOKE.ByteVectorFree(cPtr);
    return result;
}
%typemap(out) std::vector<uint8_t>& %{ $result = new std::vector<uint8_t>(*$1); %}

// Works in case when std::vector<uint8_t> is returned, but does not work when
// reference or const reference to std::vector<uint8_t> is returned.
// %typemap(out) std::vector<uint8_t> %{ $result = new std::vector<uint8_t>(std::move($1)); %}

%typemap(freearg) std::vector<uint8_t>
%{ delete $1; %}
%apply std::vector<uint8_t> { std::vector<uint8_t> const & };
// %apply std::vector<uint8_t> { std::vector<uint8_t> & };

// Handle standard C++ types
%include "arrays_csharp.i"
%include "std_string.i"
%include "std_string_view.i"
%include "std_vector.i"
%include "std_map.i"

%include "typemaps.i"

%typemap(imtype,
  inattributes="[global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPUTF8Str)]",
  outattributes="[return: global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPUTF8Str)]")
  std::string, const std::string & "string"

%ignore libcdoc::MultiDataSource;
%ignore libcdoc::MultiDataConsumer;
%ignore libcdoc::ChainedConsumer;
%ignore libcdoc::ChainedSource;
%ignore libcdoc::IStreamSource;
%ignore libcdoc::OStreamConsumer;
%ignore libcdoc::VectorConsumer;
%ignore libcdoc::VectorSource;
%ignore libcdoc::FileListConsumer;
%ignore libcdoc::FileListSource;

%ignore libcdoc::CDocWriter::createWriter(int version, DataConsumer *dst, bool take_ownership, Configuration *conf, CryptoBackend *crypto, NetworkBackend *network);
%ignore libcdoc::CDocWriter::createWriter(int version, std::ostream& ofs, Configuration *conf, CryptoBackend *crypto, NetworkBackend *network);
%ignore libcdoc::CDocWriter::encrypt(MultiDataSource& src, const std::vector<libcdoc::Recipient>& recipients);

%ignore libcdoc::CDocReader::getFMK(std::vector<uint8_t>& fmk, const libcdoc::Lock& lock);
%ignore libcdoc::CDocReader::nextFile(std::string& name, int64_t& size);
%ignore libcdoc::CDocReader::getLockForCert(Lock& lock, const std::vector<uint8_t>& cert);
%ignore libcdoc::CDocReader::decrypt(const std::vector<uint8_t>& fmk, MultiDataConsumer *consumer);
%ignore libcdoc::CDocReader::createReader(std::istream& ifs, Configuration *conf, CryptoBackend *crypto, NetworkBackend *network);

%ignore libcdoc::Configuration::KEYSERVER_SEND_URL;
%ignore libcdoc::Configuration::KEYSERVER_FETCH_URL;

%ignore libcdoc::PKCS11Backend::Handle;
%ignore libcdoc::PKCS11Backend::findCertificates(const std::string& label);
%ignore libcdoc::PKCS11Backend::findSecretKeys(const std::string& label);
%ignore libcdoc::PKCS11Backend::findCertificates(const std::vector<uint8_t>& public_key);
%ignore libcdoc::PKCS11Backend::getCertificate(std::vector<uint8_t>& val, bool& rsa, int slot, const std::vector<uint8_t>& pin, const std::vector<uint8_t>& id, const std::string& label);
%ignore libcdoc::PKCS11Backend::getPublicKey(std::vector<uint8_t>& val, bool& rsa, int slot, const std::vector<uint8_t>& pin, const std::vector<uint8_t>& id, const std::string& label);


%typemap(csdirectorin) (uint8_t *dst, size_t size) "$csinput"

//
// CDocWriter
//

// %apply unsigned char INPUT[] { unsigned char *src }

%extend libcdoc::CDocWriter {
    int64_t writeData(const uint8_t *src, size_t pos, size_t size) {
        return $self->writeData(src + pos, size);
    }
};

//
// CDocReader
//

// Use LockVector object to encapsulate the vector of locks
%template(LockVector) std::vector<libcdoc::Lock>;

// Custom wrapper do away with const qualifiers
%extend libcdoc::CDocReader {
    std::vector<libcdoc::Lock> getLocks() {
        static const std::vector<libcdoc::Lock> locks = $self->getLocks();
        std::vector<libcdoc::Lock> p(locks.cbegin(), locks.cend());
        return std::move(p);
    }
    std::vector<uint8_t> getFMK(unsigned int lock_idx) {
        std::vector<uint8_t> fmk;
        $self->getFMK(fmk, lock_idx);
        return fmk;
    }
};

%ignore libcdoc::CDocReader::getLocks();

%typemap(cscode) libcdoc::CDocReader %{
    public void readFile(FileStream ofs) {
        byte[] buf = new byte[1024];
        long result = readData(buf);
        while(result > 0) {
            ofs.Write(buf, 0, (int)result);
            result = readData(buf);
        }
    }
%}



//
// DataBuffer
//

%ignore libcdoc::DataBuffer::data;
%ignore libcdoc::DataBuffer::DataBuffer(std::vector<uint8_t> *_data);
%ignore libcdoc::DataBuffer::reset();

//
// DataConsumer
//

%ignore libcdoc::DataConsumer::write(const std::vector<uint8_t>& src);

//
// CertificateList
//

%ignore libcdoc::CertificateList::data;
%ignore libcdoc::CertificateList::CertificateList(std::vector<std::vector<uint8_t>> *_data);
%ignore libcdoc::CertificateList::reset();
%ignore libcdoc::CertificateList::setData(const std::vector<std::vector<uint8_t>>& _data);
%ignore libcdoc::CertificateList::getData();

//
// Recipient
//

%ignore libcdoc::Recipient::rcpt_key;
%ignore libcdoc::Recipient::cert;
%ignore libcdoc::Recipient::buildLabel(std::vector<std::pair<std::string_view, std::string_view>> components);
%extend libcdoc::Recipient {
    std::vector<uint8_t> getRcptKey() {
    return $self->rcpt_key;
}
void setRcptKey(const std::vector<uint8_t>& key) {
    $self->rcpt_key = key;
}
std::vector<uint8_t> getCert() {
    return $self->cert;
}
void setCert(const std::vector<uint8_t>& value) {
    $self->cert = value;
}
static std::string buildLabel(const std::vector<std::string>& values) {
    std::vector<std::pair<std::string_view, std::string_view>> vec;
    for (size_t i = 0; (i + 1) < values.size(); i += 2) {
        vec.push_back({values[i], values[i + 1]});
    }
    return libcdoc::Recipient::buildLabel(vec);
}
};

//
// Lock
//

%ignore libcdoc::Lock::Lock;
%ignore libcdoc::Lock::type;
%ignore libcdoc::Lock::pk_type;
%ignore libcdoc::Lock::label;
%ignore libcdoc::Lock::encrypted_fmk;
%ignore libcdoc::Lock::setBytes;
%ignore libcdoc::Lock::setString;
%ignore libcdoc::Lock::setInt;
%ignore libcdoc::Lock::setCertificate;
%extend libcdoc::Lock {
    Type getType() {
        return $self->type;
}
    PKType getPKType() {
        return $self->pk_type;
    }
    std::string getLabel() {
        return $self->label;
    }
    std::vector<uint8_t> getEncryptedFMK() {
        return $self->encrypted_fmk;
    }
}

//
// Configuration
//

%typemap(cscode) libcdoc::Configuration %{
public static readonly string KEYSERVER_SEND_URL = "KEYSERVER_SEND_URL";
public static readonly string KEYSERVER_FETCH_URL = "KEYSERVER_FETCH_URL";
%}

//
// CryptoBackend
//

//
// NetworkBackend
//


%feature("director") libcdoc::DataSource;
%feature("director") libcdoc::CryptoBackend;
%feature("director") libcdoc::PKCS11Backend;
%feature("director") libcdoc::NetworkBackend;
%feature("director") libcdoc::Configuration;


// Swig does not like visibility/declspec attributes
#define CDOC_EXPORT
// fixme: Remove this in production
#define LIBCDOC_TESTING 1
#define CDOC_DISABLE_MOVE(x)

%include "CDoc.h"
%include "Wrapper.h"
%include "Io.h"
%include "Recipient.h"
%include "Lock.h"
%include "Configuration.h"
%include "CryptoBackend.h"
%include "NetworkBackend.h"
%include "PKCS11Backend.h"

%include "CDocReader.h"
%include "CDocWriter.h"
