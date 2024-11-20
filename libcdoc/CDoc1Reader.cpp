#define __CDOC1_READER_CPP__

#include <iostream>
#include <map>
#include <set>

#include <openssl/x509.h>

#include "Certificate.h"

#include "CDoc.h"
#include "Crypto.h"
#include "DDocReader.h"
#include "XmlReader.h"
#include "ZStream.h"

#include "CDoc1Reader.h"

static const std::string MIME_ZLIB = "http://www.isi.edu/in-noes/iana/assignments/media-types/application/zip";
static const std::string MIME_DDOC = "http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd";
static const std::string MIME_DDOC_OLD = "http://www.sk.ee/DigiDoc/1.3.0/digidoc.xsd";

#define SCOPE(TYPE, VAR, DATA) std::unique_ptr<TYPE,decltype(&TYPE##_free)> VAR(DATA, TYPE##_free)

static const std::set<std::string> SUPPORTED_METHODS = {
	libcdoc::Crypto::AES128CBC_MTH, libcdoc::Crypto::AES192CBC_MTH, libcdoc::Crypto::AES256CBC_MTH, libcdoc::Crypto::AES128GCM_MTH, libcdoc::Crypto::AES192GCM_MTH, libcdoc::Crypto::AES256GCM_MTH
};

const std::set<std::string_view> SUPPORTED_KWAES = {
	libcdoc::Crypto::KWAES128_MTH, libcdoc::Crypto::KWAES192_MTH, libcdoc::Crypto::KWAES256_MTH
};

/**
 * @class CDoc1Reader
 * @brief CDoc1Reader is used for decrypt data.
 */

class CDoc1Reader::Private
{
public:
	struct Key
	{
		std::string id, recipient, name;
		std::string method, agreement, derive, concatDigest;
		std::vector<uint8_t> cert, publicKey, cipher;
		std::vector<uint8_t> AlgorithmID, PartyUInfo, PartyVInfo;
	};
	struct File
	{
		std::string name, size, mime, id;
	};

	std::string file, mime, method;
	std::vector<Key> _keys;
	std::vector<libcdoc::Lock *> locks;
	std::vector<File> files;
	std::map<std::string,std::string> properties;
};

const std::vector<const libcdoc::Lock>
CDoc1Reader::getLocks()
{
	std::vector<const libcdoc::Lock> locks;
	for (libcdoc::Lock *l : d->locks) locks.push_back(*l);
	return locks;
}

bool
CDoc1Reader::getLockForCert(libcdoc::Lock& lock, const std::vector<uint8_t>& cert)
{
	if (!SUPPORTED_METHODS.contains(d->method)) return false;
	libcdoc::Certificate cc(cert);
	for(const libcdoc::Lock *ll : d->locks) {
		if (!ll->isCDoc1()) continue;
		std::vector<uint8_t> cert = ll->getBytes(libcdoc::Lock::Params::CERT);
		if(cert != cc.cert || ll->encrypted_fmk.empty()) continue;
		if(cc.getAlgorithm() == libcdoc::Certificate::RSA) {
			if (ll->getString(libcdoc::Lock::Params::METHOD) == libcdoc::Crypto::RSA_MTH) {
				lock = *ll;
				return true;
			}
		} else {
			std::vector<uint8_t> eph_key = ll->getBytes(libcdoc::Lock::Params::KEY_MATERIAL);
			if(!eph_key.empty() && SUPPORTED_KWAES.contains(ll->getString(libcdoc::Lock::Params::METHOD))) {
				lock = *ll;
				return true;
			}
		}
	}
	return false;
}

int
CDoc1Reader::getFMK(std::vector<uint8_t>& fmk, const libcdoc::Lock& lock)
{
	if (lock.type != libcdoc::Lock::Type::CDOC1) {
		setLastError("Not a CDoc1 key");
		return libcdoc::UNSPECIFIED_ERROR;
	}
	setLastError({});
	std::vector<uint8_t> decrypted_key;
	if (lock.isRSA()) {
		int result = crypto->decryptRSA(decrypted_key, lock.encrypted_fmk, false, lock.label);
		if (result < 0) {
			setLastError(crypto->getLastErrorStr(result));
			return libcdoc::CRYPTO_ERROR;
		}
	} else {
		std::vector<uint8_t> eph_key = lock.getBytes(libcdoc::Lock::Params::KEY_MATERIAL);
        int result = crypto->deriveConcatKDF(decrypted_key, eph_key, lock.getString(libcdoc::Lock::Params::CONCAT_DIGEST),
											 lock.getBytes(libcdoc::Lock::Params::ALGORITHM_ID),
											 lock.getBytes(libcdoc::Lock::Params::PARTY_UINFO),
											 lock.getBytes(libcdoc::Lock::Params::PARTY_VINFO),
											 lock.label);
		if (result < 0) {
			setLastError(crypto->getLastErrorStr(result));
			return libcdoc::CRYPTO_ERROR;
		}
	}
	if(decrypted_key.empty()) {
		setLastError("Failed to decrypt/derive key");
		return libcdoc::CRYPTO_ERROR;
	}
	if(lock.isRSA()) {
		fmk = decrypted_key;
	} else {
		fmk = libcdoc::Crypto::AESWrap(decrypted_key, lock.encrypted_fmk, false);
	}
	if (fmk.empty()) {
		setLastError("Failed to decrypt/derive fmk");
		return libcdoc::CRYPTO_ERROR;
	}
	setLastError({});
	return libcdoc::OK;
}

int
CDoc1Reader::decrypt(const std::vector<uint8_t>& fmk, libcdoc::MultiDataConsumer *dst)
{
	std::vector<uint8_t> data = this->decryptData(fmk);
	std::string mime = d->mime;
	if (d->mime == MIME_ZLIB) {
		libcdoc::VectorSource vsrc(data);
		libcdoc::ZSource zsrc(&vsrc);
		std::vector<uint8_t> tmp;
		libcdoc::VectorConsumer vcons(tmp);
		vcons.writeAll(zsrc);
		data = std::move(tmp);
		mime = d->properties["OriginalMimeType"];
	}
	libcdoc::VectorSource vsrc(data);
	if(mime == MIME_DDOC || mime == MIME_DDOC_OLD) {
		std::cerr << "Contains DDoc content" << mime;
		if (!DDOCReader::parse(&vsrc, dst)) {
			setLastError("Failed to parse DDOC file");
			return libcdoc::UNSPECIFIED_ERROR;
		}
		setLastError({});
		return libcdoc::OK;
	}
	dst->open(d->properties["Filename"], data.size());
	dst->writeAll(vsrc);
	dst->close();
	setLastError({});
	return libcdoc::OK;
}

int
CDoc1Reader::beginDecryption(const std::vector<uint8_t>& fmk)
{
	return libcdoc::NOT_IMPLEMENTED;
}

int
CDoc1Reader::finishDecryption()
{
	return libcdoc::NOT_IMPLEMENTED;
}

int
CDoc1Reader::nextFile(std::string& name, int64_t& size)
{
	return libcdoc::NOT_IMPLEMENTED;
}

int64_t
CDoc1Reader::readData(uint8_t *dst, size_t size)
{
	return libcdoc::NOT_IMPLEMENTED;
}

/**
 * CDoc1Reader constructor.
 * @param file File to open reading
 */
CDoc1Reader::CDoc1Reader(const std::string &file)
	: CDocReader(1), d(new Private)
{
	d->file = file;
	auto hex2bin = [](const std::string &in) {
		std::vector<uint8_t> out;
		char data[] = "00";
		for(std::string::const_iterator i = in.cbegin(); distance(i, in.cend()) >= 2;)
		{
			data[0] = *(i++);
			data[1] = *(i++);
			out.push_back(static_cast<uint8_t>(strtoul(data, 0, 16)));
		}
		if(out[0] == 0x00)
			out.erase(out.cbegin());
		return out;
	};

	XMLReader reader(file);
	while (reader.read()) {
		if(reader.isEndElement())
			continue;
		// EncryptedData
		else if(reader.isElement("EncryptedData"))
			d->mime = reader.attribute("MimeType");
		// EncryptedData/EncryptionMethod
		else if(reader.isElement("EncryptionMethod"))
			d->method = reader.attribute("Algorithm");
		// EncryptedData/EncryptionProperties/EncryptionProperty
		else if(reader.isElement("EncryptionProperty"))
		{
			std::string attr = reader.attribute("Name");
			std::string value = reader.readText();
			if(attr == "orig_file")
			{
				Private::File file;
				size_t pos = 0, oldpos = 0;
				file.name = value.substr(oldpos, (pos = value.find("|", oldpos)) - oldpos);
				oldpos = pos + 1;
				file.size = value.substr(oldpos, (pos = value.find("|", oldpos)) - oldpos);
				oldpos = pos + 1;
				file.mime = value.substr(oldpos, (pos = value.find("|", oldpos)) - oldpos);
				oldpos = pos + 1;
				file.id = value.substr(oldpos, (pos = value.find("|", oldpos)) - oldpos);
				d->files.push_back(file);
			}
			else
				d->properties[attr] = value;
		}
		// EncryptedData/KeyInfo/EncryptedKey
		else if(reader.isElement("EncryptedKey"))
		{
			libcdoc::Lock *key =new libcdoc::Lock(libcdoc::Lock::Type::CDOC1);
			//key.id = reader.attribute("Id");
			key->label = reader.attribute("Recipient");
			while(reader.read())
			{
				if(reader.isElement("EncryptedKey") && reader.isEndElement())
					break;
				else if(reader.isEndElement())
					continue;
				// EncryptedData/KeyInfo/KeyName
				//if(reader.isElement("KeyName"))
				//	key.name = reader.readText();
				// EncryptedData/KeyInfo/EncryptedKey/EncryptionMethod
				else if(reader.isElement("EncryptionMethod"))
					key->setString(libcdoc::Lock::Params::METHOD, reader.attribute("Algorithm"));
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod
				//else if(reader.isElement("AgreementMethod"))
				//	key.agreement = reader.attribute("Algorithm");
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod
				//else if(reader.isElement("KeyDerivationMethod"))
				//	key.derive = reader.attribute("Algorithm");
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams
				else if(reader.isElement("ConcatKDFParams"))
				{
					key->setBytes(libcdoc::Lock::Params::ALGORITHM_ID, hex2bin(reader.attribute("AlgorithmID")));
					key->setBytes(libcdoc::Lock::Params::PARTY_UINFO, hex2bin(reader.attribute("PartyUInfo")));
					key->setBytes(libcdoc::Lock::Params::PARTY_VINFO, hex2bin(reader.attribute("PartyVInfo")));
				}
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/KeyDerivationMethod/ConcatKDFParams/DigestMethod
				else if(reader.isElement("DigestMethod"))
					key->setString(libcdoc::Lock::Params::CONCAT_DIGEST, reader.attribute("Algorithm"));
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/AgreementMethod/OriginatorKeyInfo/KeyValue/ECKeyValue/PublicKey
				else if(reader.isElement("PublicKey"))
					key->setBytes(libcdoc::Lock::Params::KEY_MATERIAL, reader.readBase64());
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/X509Data/X509Certificate
				else if(reader.isElement("X509Certificate"))
					key->setCertificate(reader.readBase64());
				// EncryptedData/KeyInfo/EncryptedKey/KeyInfo/CipherData/CipherValue
				else if(reader.isElement("CipherValue"))
					key->encrypted_fmk = reader.readBase64();
			}
			d->locks.push_back(key);
		}
	}
}

CDoc1Reader::~CDoc1Reader()
{
	delete d;
}

/**
 * Returns decrypted data
 * @param key Transport key to used for decrypt data
 */
std::vector<uint8_t> CDoc1Reader::decryptData(const std::vector<uint8_t> &key)
{
	XMLReader reader(d->file);
	std::vector<uint8_t> data;
	int skipKeyInfo = 0;
	while (reader.read()) {
		// EncryptedData/KeyInfo
		if(reader.isElement("KeyInfo") && reader.isEndElement())
			--skipKeyInfo;
		else if(reader.isElement("KeyInfo"))
			++skipKeyInfo;
		else if(skipKeyInfo > 0)
			continue;
		// EncryptedData/CipherData/CipherValue
		else if(reader.isElement("CipherValue"))
			return libcdoc::Crypto::decrypt(d->method, key, reader.readBase64());
	}

	return data;
}
