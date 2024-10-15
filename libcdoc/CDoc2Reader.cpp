#define __CDOC2_READER_CPP__

#include <fstream>
#include <iostream>

#include "openssl/evp.h"
#include <openssl/x509.h>

#include "Certificate.h"
#include "Crypto.h"
#include "Tar.h"
#include "Utils.h"
#include "ZStream.h"
#include "CDoc2.h"

#include "header_generated.h"

#include "CDoc2Reader.h"

// fixme: Placeholder
#define t_(t) t

struct CDoc2Reader::Private {
	Private(libcdoc::DataSource *src, bool take_ownership) : _src(src), _owned(take_ownership) {
	}

	~Private() {
		if (_owned) delete _src;
	}

	libcdoc::DataSource *_src;
	bool _owned;
	size_t _nonce_pos = 0;
	bool _at_nonce = false;

	std::vector<uint8_t> header_data;
	std::vector<uint8_t> headerHMAC;

	std::vector<libcdoc::Lock *> locks;

	std::unique_ptr<libcdoc::Crypto::Cipher> cipher;
	std::unique_ptr<TaggedSource> tgs;
	std::unique_ptr<libcdoc::ZSource> zsrc;
	std::unique_ptr<libcdoc::TarSource> tar;

};

CDoc2Reader::~CDoc2Reader()
{
	for (libcdoc::Lock *lock : priv->locks) {
		delete lock;
	}
}

const std::vector<libcdoc::Lock *>&
CDoc2Reader::getLocks()
{
	return priv->locks;
}

const libcdoc::Lock *
CDoc2Reader::getDecryptionLock(const std::vector<uint8_t>& cert)
{
	libcdoc::Certificate cc(cert);
	std::vector<uint8_t> other_key = cc.getPublicKey();
	for (const libcdoc::Lock *lock : priv->locks) {
		if (lock->hasTheSameKey(other_key)) return lock;
	}
	return nullptr;
}

int
CDoc2Reader::getFMK(std::vector<uint8_t>& fmk, const libcdoc::Lock *lock)
{
	std::vector<uint8_t> kek;
	if (lock->isSymmetric()) {
		// Symmetric key
		const libcdoc::LockSymmetric &sk = static_cast<const libcdoc::LockSymmetric&>(*lock);
		std::string info_str = libcdoc::CDoc2::getSaltForExpand(sk.label);
		crypto->getKEK(kek, sk.salt, sk.pw_salt, sk.kdf_iter, sk.label, info_str);
	} else {
		// Public/private key
		const libcdoc::LockPKI &pki = static_cast<const libcdoc::LockPKI&>(*lock);
		std::vector<uint8_t> key_material;
		if(lock->type == libcdoc::Lock::Type::SERVER) {
			const libcdoc::LockServer &sk = static_cast<const libcdoc::LockServer&>(*lock);
			int result = network->fetchKey(key_material, sk.keyserver_id, sk.transaction_id);
			if (result < 0) {
				setLastError(network->getLastErrorStr(result));
				return result;
			}
		} else if (lock->type == libcdoc::Lock::PUBLIC_KEY) {
			const libcdoc::LockPublicKey& pk = static_cast<const libcdoc::LockPublicKey&>(*lock);
			key_material = pk.key_material;
		}
#ifndef NDEBUG
		std::cerr << "Public key: " << libcdoc::Crypto::toHex(pki.rcpt_key) << std::endl;
		std::cerr << "Key material: " << libcdoc::Crypto::toHex(key_material) << std::endl;
#endif
		if (pki.pk_type == libcdoc::Lock::PKType::RSA) {
			int result = crypto->decryptRSA(kek, key_material, true);
			if (result < 0) {
				setLastError(crypto->getLastErrorStr(result));
				return result;
			}
		} else {
			std::vector<uint8_t> kek_pm;
			int result = crypto->deriveHMACExtract(kek_pm, key_material, std::vector<uint8_t>(libcdoc::CDoc2::KEKPREMASTER.cbegin(), libcdoc::CDoc2::KEKPREMASTER.cend()), libcdoc::CDoc2::KEY_LEN);
			if (result < 0) {
				setLastError(crypto->getLastErrorStr(result));
				return result;
			}
#ifndef NDEBUG
			std::cerr << "Key kekPm: " << libcdoc::Crypto::toHex(kek_pm) << std::endl;
#endif
			std::string info_str = libcdoc::CDoc2::getSaltForExpand(key_material, pki.rcpt_key);
#ifndef NDEBUG
			std::cerr << "info" << libcdoc::Crypto::toHex(std::vector<uint8_t>(info_str.cbegin(), info_str.cend())) << std::endl;
#endif
			kek = libcdoc::Crypto::expand(kek_pm, std::vector<uint8_t>(info_str.cbegin(), info_str.cend()), libcdoc::CDoc2::KEY_LEN);
		}
	}
#ifndef NDEBUG
	std::cerr << "kek: " << libcdoc::Crypto::toHex(kek) << std::endl;
#endif

	if(kek.empty()) {
		setLastError(t_("Failed to derive key"));
		return false;
	}
	if (libcdoc::Crypto::xor_data(fmk, lock->encrypted_fmk, kek) != libcdoc::OK) {
		setLastError(t_("Failed to decrypt/derive fmk"));
		return libcdoc::CRYPTO_ERROR;
	}
	std::vector<uint8_t> hhk = libcdoc::Crypto::expand(fmk, std::vector<uint8_t>(libcdoc::CDoc2::HMAC.cbegin(), libcdoc::CDoc2::HMAC.cend()));
#ifndef NDEBUG
	std::cerr << "xor: " << libcdoc::Crypto::toHex(lock->encrypted_fmk) << std::endl;
	std::cerr << "fmk: " << libcdoc::Crypto::toHex(fmk) << std::endl;
	std::cerr << "hhk: " << libcdoc::Crypto::toHex(hhk) << std::endl;
	std::cerr << "hmac: " << libcdoc::Crypto::toHex(priv->headerHMAC) << std::endl;
#endif
	if(libcdoc::Crypto::sign_hmac(hhk, priv->header_data) != priv->headerHMAC) {
		setLastError(t_("CDoc 2.0 hash mismatch"));
		return libcdoc::HASH_MISMATCH;
	}
	setLastError({});
	return libcdoc::OK;
}

int
CDoc2Reader::decrypt(const std::vector<uint8_t>& fmk, libcdoc::MultiDataConsumer *consumer)
{
	int result = beginDecryption(fmk);
	if (result != libcdoc::OK) return result;
	bool warning = false;
//	libcdoc::TarSource tar(&zsrc, false);
	std::string name;
	int64_t size;
	result = nextFile(name, size);
	while (result == libcdoc::OK) {
		consumer->open(name, size);
		consumer->writeAll(*priv->tar);
		result = nextFile(name, size);
	}
	if (result != libcdoc::END_OF_STREAM) {
		setLastError(priv->tar->getLastErrorStr(result));
		return result;
	}
	return finishDecryption();
}

int
CDoc2Reader::beginDecryption(const std::vector<uint8_t>& fmk)
{
	if (!priv->_at_nonce) {
		int result = priv->_src->seek(priv->_nonce_pos);
		if (result != libcdoc::OK) {
			setLastError(priv->_src->getLastErrorStr(result));
			return libcdoc::IO_ERROR;
		}
	}
	priv->_at_nonce = false;
	std::vector<uint8_t> cek = libcdoc::Crypto::expand(fmk, std::vector<uint8_t>(libcdoc::CDoc2::CEK.cbegin(), libcdoc::CDoc2::CEK.cend()));
	std::vector<uint8_t> nonce(libcdoc::CDoc2::NONCE_LEN);
	if (priv->_src->read(nonce.data(), libcdoc::CDoc2::NONCE_LEN) != libcdoc::CDoc2::NONCE_LEN) {
		setLastError("Error reading nonce");
		return libcdoc::IO_ERROR;
	}
#ifndef NDEBUG
	std::cerr << "cek: " << libcdoc::Crypto::toHex(cek) << std::endl;
	std::cerr << "nonce: " << libcdoc::Crypto::toHex(nonce) << std::endl;
#endif
	priv->cipher = std::make_unique<libcdoc::Crypto::Cipher>(EVP_chacha20_poly1305(), cek, nonce, false);
	std::vector<uint8_t> aad(libcdoc::CDoc2::PAYLOAD.cbegin(), libcdoc::CDoc2::PAYLOAD.cend());
	aad.insert(aad.end(), priv->header_data.cbegin(), priv->header_data.cend());
	aad.insert(aad.end(), priv->headerHMAC.cbegin(), priv->headerHMAC.cend());
	if(!priv->cipher->updateAAD(aad)) {
		setLastError("OpenSSL error");
		return libcdoc::UNSPECIFIED_ERROR;
	}

	priv->tgs = std::make_unique<TaggedSource>(priv->_src, false, 16);
	libcdoc::CipherSource *csrc = new libcdoc::CipherSource(priv->tgs.get(), false, priv->cipher.get());
	priv->zsrc = std::make_unique<libcdoc::ZSource>(csrc, false);
	priv->tar = std::make_unique<libcdoc::TarSource>(priv->zsrc.get(), false);

	return libcdoc::OK;
}

int
CDoc2Reader::nextFile(std::string& name, int64_t& size)
{
	if (!priv->tar) return libcdoc::WORKFLOW_ERROR;
	return priv->tar->next(name, size);
}

int64_t
CDoc2Reader::read(uint8_t *dst, size_t size)
{
	if (!priv->tar) return libcdoc::WORKFLOW_ERROR;
	return priv->tar->read(dst, size);
}

int
CDoc2Reader::finishDecryption()
{
	if (!priv->zsrc->isEof()) {
		setLastError(t_("CDoc contains additional payload data that is not part of content"));
	}

#ifndef NDEBUG
	std::cerr << "tag: " << libcdoc::Crypto::toHex(priv->tgs->tag) << std::endl;
#endif
	priv->cipher->setTag(priv->tgs->tag);
	if (!priv->cipher->result()) {
		setLastError("Stream tag does not match");
		return libcdoc::UNSPECIFIED_ERROR;
	}
	setLastError({});
	return libcdoc::OK;
	priv->tar.reset();
	return libcdoc::OK;
}

CDoc2Reader::CDoc2Reader(libcdoc::DataSource *src, bool take_ownership)
	: CDocReader(2), priv(new Private(src, take_ownership))
{

	using namespace cdoc20::recipients;
	using namespace cdoc20::header;

	setLastError(t_("Invalid CDoc 2.0 header"));

	uint8_t in[libcdoc::CDoc2::LABEL.size()];
	if (priv->_src->read(in, libcdoc::CDoc2::LABEL.size()) != libcdoc::CDoc2::LABEL.size()) return;
	if (memcmp(libcdoc::CDoc2::LABEL.data(), in, libcdoc::CDoc2::LABEL.size())) return;
	//if (libcdoc::CDoc2::LABEL.compare(0, libcdoc::CDoc2::LABEL.size(), (const char *) in)) return;

	// Read 32-bit header length in big endian order
	uint8_t c[4];
	if (priv->_src->read(c, 4) != 4) return;
	uint32_t header_len = (c[0] << 24) | (c[1] << 16) | c[2] << 8 | c[3];
	priv->header_data.resize(header_len);
	if (priv->_src->read(priv->header_data.data(), header_len) != header_len) return;
	priv->headerHMAC.resize(libcdoc::CDoc2::KEY_LEN);
	if (priv->_src->read(priv->headerHMAC.data(), libcdoc::CDoc2::KEY_LEN) != libcdoc::CDoc2::KEY_LEN) return;

	priv->_nonce_pos = libcdoc::CDoc2::LABEL.size() + 4 + header_len + libcdoc::CDoc2::KEY_LEN;
	priv->_at_nonce = true;

	flatbuffers::Verifier verifier(priv->header_data.data(), priv->header_data.size());
	if(!VerifyHeaderBuffer(verifier)) return;
	const auto *header = GetHeader(priv->header_data.data());
	if(!header) return;
	if(header->payload_encryption_method() != PayloadEncryptionMethod::CHACHA20POLY1305) return;
	const auto *recipients = header->recipients();
	if(!recipients) return;

	setLastError({});

	for(const auto *recipient: *recipients){
		if(recipient->fmk_encryption_method() != FMKEncryptionMethod::XOR)
		{
			std::cerr << "Unsupported FMK encryption method: skipping" << std::endl;
			continue;
		}
		auto fillRecipientPK = [&] (libcdoc::Lock::PKType pk_type, auto key) {
			libcdoc::LockPublicKey *k = new libcdoc::LockPublicKey(pk_type, key->recipient_public_key()->data(), key->recipient_public_key()->size());
			k->label = recipient->key_label()->str();
			k->encrypted_fmk.assign(recipient->encrypted_fmk()->cbegin(), recipient->encrypted_fmk()->cend());
			return k;
		};
		switch(recipient->capsule_type())
		{
		case Capsule::ECCPublicKeyCapsule:
			if(const auto *key = recipient->capsule_as_ECCPublicKeyCapsule()) {
				if(key->curve() != EllipticCurve::secp384r1) {
					std::cerr << "Unsupported ECC curve: skipping" << std::endl;
					continue;
				}
				libcdoc::LockPublicKey *k = fillRecipientPK(libcdoc::Lock::PKType::ECC, key);
				k->key_material.assign(key->sender_public_key()->cbegin(), key->sender_public_key()->cend());
				std::cerr << "Load PK: " << libcdoc::Crypto::toHex(k->rcpt_key) << std::endl;
				priv->locks.push_back(k);
			}
			break;
		case Capsule::RSAPublicKeyCapsule:
			if(const auto *key = recipient->capsule_as_RSAPublicKeyCapsule())
			{
				libcdoc::LockPublicKey *k = fillRecipientPK(libcdoc::Lock::PKType::RSA, key);
				k->key_material.assign(key->encrypted_kek()->cbegin(), key->encrypted_kek()->cend());
				priv->locks.push_back(k);
			}
			break;
		case Capsule::KeyServerCapsule:
			if (const KeyServerCapsule *server = recipient->capsule_as_KeyServerCapsule()) {
				KeyDetailsUnion details = server->recipient_key_details_type();
				libcdoc::LockServer *ckey = nullptr;
				switch (details) {
				case KeyDetailsUnion::EccKeyDetails:
					if(const EccKeyDetails *eccDetails = server->recipient_key_details_as_EccKeyDetails()) {
						if(eccDetails->curve() == EllipticCurve::secp384r1) {
							ckey = libcdoc::LockServer::fromKey(std::vector<uint8_t>(eccDetails->recipient_public_key()->cbegin(), eccDetails->recipient_public_key()->cend()), libcdoc::Lock::PKType::ECC);
						} else {
							std::cerr << "Unsupported elliptic curve key type" << std::endl;
						}
					} else {
						std::cerr << "Invalid file format" << std::endl;
					}
					break;
				case KeyDetailsUnion::RsaKeyDetails:
					if(const RsaKeyDetails *rsaDetails = server->recipient_key_details_as_RsaKeyDetails()) {
						ckey = libcdoc::LockServer::fromKey(std::vector<uint8_t>(rsaDetails->recipient_public_key()->cbegin(), rsaDetails->recipient_public_key()->cend()), libcdoc::Lock::PKType::RSA);
					} else {
						std::cerr << "Invalid file format" << std::endl;
					}
					break;
				default:
					std::cerr << "Unsupported Key Server Details: skipping" << std::endl;
				}
				if (ckey) {
					ckey->label = recipient->key_label()->c_str();
					ckey->encrypted_fmk.assign(recipient->encrypted_fmk()->cbegin(), recipient->encrypted_fmk()->cend());
					ckey->keyserver_id = server->keyserver_id()->str();
					ckey->transaction_id = server->transaction_id()->str();
					priv->locks.push_back(ckey);
				}
			} else {
				std::cerr << "Invalid file format" << std::endl;
			}
			break;
		case Capsule::SymmetricKeyCapsule:
			if(const auto *capsule = recipient->capsule_as_SymmetricKeyCapsule())
			{
				libcdoc::LockSymmetric *key = new libcdoc::LockSymmetric(std::vector<uint8_t>(capsule->salt()->cbegin(), capsule->salt()->cend()));
				key->label = recipient->key_label()->str();
				key->encrypted_fmk.assign(recipient->encrypted_fmk()->cbegin(), recipient->encrypted_fmk()->cend());
				priv->locks.push_back(key);
			}
			break;
		case Capsule::PBKDF2Capsule:
			if(const auto *capsule = recipient->capsule_as_PBKDF2Capsule()) {
				KDFAlgorithmIdentifier kdf_id = capsule->kdf_algorithm_identifier();
				if (kdf_id != KDFAlgorithmIdentifier::PBKDF2WithHmacSHA256) {
					std::cerr << "Unsupported KDF algorithm: skipping" << std::endl;
					continue;
				}
				auto salt = capsule->salt();
				auto pw_salt = capsule->password_salt();
				int32_t kdf_iter = capsule->kdf_iterations();
				libcdoc::LockSymmetric *key = new libcdoc::LockSymmetric(std::vector<uint8_t>(salt->cbegin(), salt->cend()));
				key->label = recipient->key_label()->str();
				key->encrypted_fmk.assign(recipient->encrypted_fmk()->cbegin(), recipient->encrypted_fmk()->cend());
				key->pw_salt.assign(pw_salt->cbegin(), pw_salt->cend());
				key->kdf_iter = kdf_iter;
				priv->locks.push_back(key);
			}
			break;
		default:
			std::cerr << "Unsupported Key Details: skipping" << std::endl;
		}
	}
}

CDoc2Reader::CDoc2Reader(const std::string &path)
	: CDoc2Reader(new libcdoc::IStreamSource(path), true)
{
}

bool
CDoc2Reader::isCDoc2File(const std::string& path)
{
	std::ifstream fb(path);
	std::cerr << "A";
	char in[libcdoc::CDoc2::LABEL.size()];
	std::cerr << "B";
	if (!fb.read(in, libcdoc::CDoc2::LABEL.size()) || (fb.gcount() != libcdoc::CDoc2::LABEL.size())) return false;
	std::cerr << "C";
	if (libcdoc::CDoc2::LABEL.compare(0, libcdoc::CDoc2::LABEL.size(), in)) return false;
	std::cerr << "D";
	return true;
}

