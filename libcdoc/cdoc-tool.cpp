#define __CDOC_TOOL_CPP__

#include <cstring>
#include <iostream>
#include <sstream>
#include <map>

#include "CDocReader.h"
#include "CDocWriter.h"
#include "CDoc.h"
#include "PKCS11Backend.h"
#include "Utils.h"
#include "Crypto.h"

using namespace std;

struct RcptInfo {
	enum Type {
        // Detect type from container
        ANY,
        CERT,
        PASSWORD,
        // Symetric key from command line
        SKEY,
        // Public key from command line
        PKEY,
        // Symetric key from PKCS11 device
        P11_SYMMETRIC,
        // Public key from PKC11 device
        P11_PKI
	};
	Type type;
	std::vector<uint8_t> cert;
    /* Pin or password */
	std::vector<uint8_t> secret;
	long slot = 0;
    //std::string pin;
	std::vector<uint8_t> key_id;
	std::string key_label;
};

//
//
//
//


static void
print_usage(ostream& ofs)
{
    ofs << "cdoc-tool encrypt [--library PKCS11LIBRARY] --rcpt RECIPIENT [--rcpt...] -v1 --out OUTPUTFILE FILE [FILE...]" << endl;
    ofs << "  Encrypt files for one or more recipients" << endl;
    ofs << "  RECIPIENT has to be one of the following:" << endl;
    ofs << "    label:cert:CERTIFICATE_HEX - public key from certificate" << endl;
    ofs << "    label:skey:SECRET_KEY_HEX - AES key" << endl;
    ofs << "    label:pkey:SECRET_KEY_HEX - public key" << endl;
    ofs << "    label:pfkey:PUB_KEY_FILE - path to DER file with EC (secp384r1 curve) public key" << endl;
    ofs << "    label:pw:PASSWORD - Derive key using PWBKDF" << endl;
    ofs << "    label:p11sk:SLOT:[PIN]:[PKCS11 ID]:[PKCS11 LABEL] - use AES key from PKCS11 module" << endl;
    ofs << "    label:p11pk:SLOT:[PIN]:[PKCS11 ID]:[PKCS11 LABEL] - use public key from PKCS11 module" << endl;
    ofs << "  -v1 creates CDOC1 version container. Supported only on encryption with certificate." << endl;
    ofs << endl;
    ofs << "cdoc-tool decrypt [--library LIBRARY] ARGUMENTS FILE [OUTPU_DIR]" << endl;
    ofs << "  Decrypt container using lock specified by label" << endl;
    ofs << "  Supported arguments" << endl;
    ofs << "    --label LABEL   CDoc container lock label" << endl;
    ofs << "    --slot SLOT     PKCS11 slot number" << endl;
    ofs << "    --secret|password|pin SECRET    Secret phrase (either lock password or PKCS11 pin)" << endl;
    ofs << "    --key-id        PKCS11 key id" << endl;
    ofs << "    --key-label     PKCS11 key label" << endl;
    ofs << "    --library       path to the PKCS11 library to be used" << endl;
    ofs << endl;
    ofs << "cdoc-tool locks FILE" << endl;
    ofs << "  Show locks in a container file" << endl;

    //<< "cdoc-tool encrypt -r X509DerRecipientCert [-r X509DerRecipientCert [...]] InFile [InFile [...]] OutFile" << std::endl
    //	<< "cdoc-tool encrypt --rcpt RECIPIENT [--rcpt RECIPIENT] [--file INFILE] [...] --out OUTFILE" << std::endl
    //	<< "  where RECIPIENT is in form label:TYPE:value" << std::endl
    //	<< "    where TYPE is 'cert', 'key' or 'pw'" << std::endl
#ifdef _WIN32
    //	<< "cdoc-tool decrypt win [ui|noui] pin InFile OutFolder" << endl
#endif
    //	<< "cdoc-tool decrypt pkcs11 path/to/so pin InFile OutFolder" << std::endl
    //	<< "cdoc-tool decrypt pkcs12 path/to/pkcs12 pin InFile OutFolder" << std::endl;
}

static std::vector<uint8_t>
fromHex(const std::string& hex) {
	std::vector<uint8_t> val(hex.size() / 2);
	char c[3] = {0};
	for (size_t i = 0; i < (hex.size() & 0xfffffffe); i += 2) {
		std::copy(hex.cbegin() + i, hex.cbegin() + i + 2, c);
		val[i / 2] = (uint8_t) strtol(c, NULL, 16);
	}
	return std::move(val);
}

static std::vector<std::string>
split (const std::string &s, char delim = ':') {
	std::vector<std::string> result;
	std::stringstream ss(s);
	std::string item;
	while (getline (ss, item, delim)) {
		result.push_back (item);
	}
	return result;
}

static std::vector<uint8_t>
fromStr(const std::string& str) {
	return std::vector<uint8_t>(str.cbegin(), str.cend());
}

struct ToolConf : public libcdoc::Configuration {
	std::string getValue(const std::string& param) override final {
		return "false";
	}
};

struct ToolPKCS11 : public libcdoc::PKCS11Backend {
	const std::map<std::string, RcptInfo>& rcpts;

	ToolPKCS11(const std::string& library, const std::map<std::string, RcptInfo>& map) : libcdoc::PKCS11Backend(library), rcpts(map) {}

    int connectToKey(const std::string& label, bool priv) override final {
        if (!rcpts.contains(label)) return libcdoc::CRYPTO_ERROR;
		const RcptInfo& rcpt = rcpts.at(label);
		int result = libcdoc::CRYPTO_ERROR;
        if (!priv) {
            result = useSecretKey(rcpt.slot, rcpt.secret, rcpt.key_id, rcpt.key_label);
        } else {
            result = usePrivateKey(rcpt.slot, rcpt.secret, rcpt.key_id, rcpt.key_label);
		}
		if (result != libcdoc::OK) return result;
		return libcdoc::OK;
	}
};

struct ToolCrypto : public libcdoc::CryptoBackend {
	std::map<std::string, RcptInfo> rcpts;
    std::unique_ptr<libcdoc::CryptoBackend> p11;

	ToolCrypto() = default;

	bool connectLibrary(const std::string& library) {
		p11 = std::make_unique<ToolPKCS11>(library, rcpts);
		return true;
	}

	int decryptRSA(std::vector<uint8_t>& dst, const std::vector<uint8_t> &data, bool oaep, const std::string& label) override final {
		if (p11) return p11->decryptRSA(dst, data, oaep, label);
		return libcdoc::NOT_IMPLEMENTED;
	}
	int deriveConcatKDF(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::string &digest,
		const std::vector<uint8_t> &algorithmID, const std::vector<uint8_t> &partyUInfo, const std::vector<uint8_t> &partyVInfo, const std::string& label) override final {
		if (p11) return p11->deriveConcatKDF(dst, publicKey, digest, algorithmID, partyUInfo, partyVInfo, label);
		return libcdoc::NOT_IMPLEMENTED;
	}
	int deriveHMACExtract(std::vector<uint8_t>& dst, const std::vector<uint8_t> &publicKey, const std::vector<uint8_t> &salt, const std::string& label) override final {
		if (p11) return p11->deriveHMACExtract(dst, publicKey, salt, label);
		return libcdoc::NOT_IMPLEMENTED;
	}
	int extractHKDF(std::vector<uint8_t>& kek, const std::vector<uint8_t>& salt, const std::vector<uint8_t> pw_salt, int32_t kdf_iter, const std::string& label) override {
		if (p11) return p11->extractHKDF(kek, salt, pw_salt, kdf_iter, label);
		return libcdoc::CryptoBackend::extractHKDF(kek, salt, pw_salt, kdf_iter, label);
	}
	int getSecret(std::vector<uint8_t>& secret, const std::string& label) override final {
		const RcptInfo& rcpt = rcpts.at(label);
		secret =rcpt.secret;
		return (secret.empty()) ? INVALID_PARAMS : libcdoc::OK;
	}
};

#define PUSH true

//
// cdoc-tool encrypt --rcpt RECIPIENT [--rcpt...] --out OUTPUTFILE FILE [FILE...]
// Where RECIPIENT has a format:
//   label:cert:CERTIFICATE_HEX
//	 label:key:SECRET_KEY_HEX
//   label:pw:PASSWORD
//	 label:p11sk:SLOT:[PIN]:[ID]:[LABEL]
//	 label:p11pk:SLOT:[PIN]:[ID]:[LABEL]
//

int encrypt(int argc, char *argv[])
{
    std::cout << "Encrypting" << std::endl;

	ToolCrypto crypto;

    bool libraryRequired = false;
	std::string library;
	std::vector<std::string> files;
	std::string out;
    int cdocVersion = 2;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--rcpt") && ((i + 1) <= argc)) {
			std::vector<std::string> parts = split(argv[i + 1]);
            if (parts.size() < 3)
            {
                print_usage(std::cerr);
                return 1;
            }
            const string& label = parts[0];
            const string& method = parts[1];
            if (method == "cert") {
                if (parts.size() != 3)
                {
                    print_usage(std::cerr);
                    return 1;
                }
				crypto.rcpts[label] = {
					RcptInfo::CERT,
					libcdoc::readFile(libcdoc::toUTF8(parts[2])),
					{}
				};
            }
            else if (method == "key" || method == "skey" || method == "pkey")
            {
                // For backward compatibility leave also "key" as the synonym for "skey" method.
                if (parts.size() != 3)
                {
                    print_usage(cerr);
                    return 1;
                }

                RcptInfo::Type type = method == "pkey" ? RcptInfo::PKEY : RcptInfo::SKEY;

				crypto.rcpts[label] = {
                    type,
					{},
					fromHex(parts[2])
				};
            }
            else if (method == "pfkey")
            {
                if (parts.size() != 3)
                {
                    print_usage(cerr);
                    return 1;
                }

                filesystem::path keyFilePath(parts[2]);
                if (!filesystem::exists(keyFilePath))
                {
                    cerr << "Key file '" << keyFilePath << "' does not exist" << endl;
                    return 1;
                }
                ifstream keyStream(keyFilePath, ios_base::in | ios_base::binary);
                if (!keyStream)
                {
                    cerr << "File '" << keyFilePath << "' opening failed." << endl;
                    return 1;
                }

                // ifstream::char_type firstChr;
                // keyStream.get(firstChr);
                // if (keyStream.bad())
                // {
                //     cerr << "File '" << keyFile << "' reading failed." << endl;
                //     return 1;
                // }

                // DER files begin usually with 0x04 byte
                // if (firstChr != 0x04)
                // {
                //     cerr << "File '" << keyFile << "' does not seem to be a DER file." << endl;
                //     return 1;
                // }

                // Determine the file size
                keyStream.seekg(0, ios_base::end);
                size_t length = keyStream.tellg();

                // Read the file
                vector<uint8_t> key(length);
                keyStream.seekg(0);
                keyStream.read(reinterpret_cast<ifstream::char_type*>(key.data()), length);

                crypto.rcpts[label] = {
                    RcptInfo::PKEY,
                    {},
                    key
                };
            }
            else if (method == "pw")
            {
                if (parts.size() != 3)
                {
                    print_usage(std::cerr);
                    return 1;
                }
				crypto.rcpts[label] = {
					RcptInfo::PASSWORD,
					{},
					std::vector<uint8_t>(parts[2].cbegin(), parts[2].cend())
				};
            }
            else if (method == "p11sk" || method == "p11pk")
            {
                RcptInfo::Type type = method == "p11sk" ? RcptInfo::P11_SYMMETRIC : RcptInfo::P11_PKI;

                if (parts.size() < 5) {
                    print_usage(std::cerr);
                    return 1;
                }
                libraryRequired = true;
				long slot;
				if (parts[2].starts_with("0x")) {
					slot = std::stol(parts[2].substr(2), nullptr, 16);
				} else {
					slot = std::stol(parts[2]);
				}
				std::string& pin = parts[3];
				std::vector<uint8_t> key_id = fromHex(parts[4]);
				std::string key_label = (parts.size() >= 6) ? parts[5] : "";
				crypto.rcpts[label] = {
                    type,
                    {}, std::vector<uint8_t>(pin.cbegin(), pin.cend()),
                    slot, key_id, key_label
				};

#ifndef NDEBUG
                // For debugging
                cout << "Method: " << method << endl;
                cout << "Slot: " << slot << endl;
                if (!pin.empty())
                    cout << "Pin: " << pin << endl;
                if (!key_id.empty())
                    cout << "Key ID: " << parts[4] << endl;
                if (!key_label.empty())
                    cout << "Key label: " << key_label << endl;
#endif
            }
            else
            {
                cerr << "Unkown method: " << method << endl;
                print_usage(cerr);
                return 1;
			}
			i += 1;
		} else if (!strcmp(argv[i], "--out") && ((i + 1) <= argc)) {
			out = argv[i + 1];
			i += 1;
		} else if (!strcmp(argv[i], "--library") && ((i + 1) <= argc)) {
			library = argv[i + 1];
			i += 1;
        }
        else if (!strcmp(argv[i], "-v1"))
        {
            cdocVersion = 1;
            i++;
        }
        else if (argv[i][0] == '-') {
            print_usage(std::cerr);
            return 1;
		} else {
			files.push_back(argv[i]);
		}
	}
	if (crypto.rcpts.empty()) {
		std::cerr << "No recipients" << std::endl;
        print_usage(std::cerr);
        return 1;
	}
	if (files.empty()) {
		std::cerr << "No files specified" << std::endl;
        print_usage(std::cerr);
        return 1;
	}
	if (out.empty()) {
		std::cerr << "No output specified" << std::endl;
        print_usage(std::cerr);
        return 1;
	}

    // CDOC1 is supported only in case of encryption with certificate.
    if (cdocVersion == 1)
    {
        for (const pair<string, RcptInfo>& rcpt : crypto.rcpts)
        {
            if (rcpt.second.type != RcptInfo::CERT)
            {
                cerr << "CDOC version 1 container can be used on encryption with certificate only." << endl;
                print_usage(cerr);
                return 1;
            }
        }
    }

    if (!library.empty())
    {
        crypto.connectLibrary(library);
    }
    else if (libraryRequired)
    {
        cerr << "Cryptographic library is required" << endl;
        print_usage(cerr);
        return 1;
    }

	std::vector<libcdoc::Recipient> keys;
    for (const std::pair<std::string, RcptInfo>& pair : crypto.rcpts) {
        const std::string& label = pair.first;
		const RcptInfo& rcpt = pair.second;
		libcdoc::Recipient key;
        if (rcpt.type == RcptInfo::Type::CERT)
        {
			key = libcdoc::Recipient::makeCertificate(label, rcpt.cert);
        }
        else if (rcpt.type == RcptInfo::Type::SKEY) {
			key = libcdoc::Recipient::makeSymmetric(label, 0);
			std::cerr << "Creating symmetric key:" << std::endl;
        }
        else if (rcpt.type == RcptInfo::Type::PKEY)
        {
            key = libcdoc::Recipient::makePublicKey(label, rcpt.secret, libcdoc::Recipient::PKType::ECC);
            std::cerr << "Creating public key:" << std::endl;
        }
        else if (rcpt.type == RcptInfo::Type::P11_SYMMETRIC)
        {
			key = libcdoc::Recipient::makeSymmetric(label, 0);
        }
        else if (rcpt.type == RcptInfo::Type::P11_PKI)
        {
			std::vector<uint8_t> val;
			bool rsa;
            ToolPKCS11* p11 = dynamic_cast<ToolPKCS11*>(crypto.p11.get());
            int result = p11->getPublicKey(val, rsa, rcpt.slot, rcpt.secret, rcpt.key_id, rcpt.key_label);
			if (result != libcdoc::OK) {
				std::cerr << "No such public key: " << rcpt.key_label << std::endl;
				continue;
			}
			std::cerr << "Public key (" << (rsa ? "rsa" : "ecc") << "):" << libcdoc::Crypto::toHex(val) << std::endl;
			key = libcdoc::Recipient::makePublicKey(label, val, rsa ? libcdoc::Recipient::PKType::RSA : libcdoc::Recipient::PKType::ECC);
        }
        else if (rcpt.type == RcptInfo::Type::PASSWORD)
        {
			std::cerr << "Creating password key:" << std::endl;
			key = libcdoc::Recipient::makeSymmetric(label, 65535);
		}

		keys.push_back(key);
	}

    if (keys.empty())
    {
        cerr << "No key for encryption was found" << endl;
        return 1;
    }

    ToolConf conf;
    unique_ptr<libcdoc::CDocWriter> writer(libcdoc::CDocWriter::createWriter(cdocVersion, out, &conf, &crypto, nullptr));

	if (PUSH) {
        writer->beginEncryption();
		for (const libcdoc::Recipient& rcpt : keys) {
			writer->addRecipient(rcpt);
		}
		for (const std::string& file : files) {
			std::filesystem::path path(file);
			if (!std::filesystem::exists(path)) {
                cerr << "File does not exist: " << file << endl;
				return 1;
			}
			size_t size = std::filesystem::file_size(path);
			writer->addFile(file, size);
			libcdoc::IStreamSource src(file);
			while (!src.isEof()) {
				uint8_t b[256];
				int64_t len = src.read(b, 256);
				if (len < 0) {
					std::cerr << "IO error: " << file;
					return 1;
				}
				writer->writeData(b, len);
			}
		}
		writer->finishEncryption();
	} else {
		libcdoc::FileListSource src({}, files);
		writer->encrypt(src, keys);
	}

	return 0;
}

//
// cdoc-tool decrypt ARGUMENTS FILE [OUTPU_DIR]
//   --label LABEL   CDoc container lock label
//   --slot SLOT     PKCS11 slot number
//   --secret|password|pin SECRET    Secret phrase (either lock password or PKCS11 pin)
//   --key-id        PKCS11 key id
//   --key-label     PKCS11 key label
//   --library       full path to cryptographic library to be used (needed for decryption with PKCS11)

int decrypt(int argc, char *argv[])
{
	ToolCrypto crypto;
    std::string library;

	std::string label;
	std::vector<uint8_t> secret;
    long slot;
    std::vector<uint8_t> key_id;
    std::string key_label;
    std::string file;
    std::string basePath;
    bool libraryRequired = false;
    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "--label") && ((i + 1) < argc)) {
			label = argv[i + 1];
			i += 1;
        } else if (!strcmp(argv[i], "--password") || !strcmp(argv[i], "--secret") || !strcmp(argv[i], "--pin")) {
            if ((i + 1) >= argc) {
                print_usage(cerr);
                return 1;
            }
            string_view s(argv[i + 1]);
            secret.assign(s.cbegin(), s.cend());
			i += 1;
        } else if (!strcmp(argv[i], "--slot")) {
            if ((i + 1) >= argc) {
                print_usage(cerr);
                return 1;
            }
            libraryRequired = true;
            string str(argv[i + 1]);
            if (str.starts_with("0x")) {
                slot = std::stol(str.substr(2), nullptr, 16);
            } else {
                slot = std::stol(str);
            }
            i += 1;
        } else if (!strcmp(argv[i], "--key-id")) {
            if ((i + 1) >= argc) {
                print_usage(cerr);
                return 1;
            }
            string_view s(argv[i + 1]);
            key_id.assign(s.cbegin(), s.cend());
            i += 1;
        } else if (!strcmp(argv[i], "--key-label")) {
            if ((i + 1) >= argc) {
                print_usage(cerr);
                return 1;
            }
            string_view s(argv[i + 1]);
            key_label.assign(s.cbegin(), s.cend());
            i += 1;
        } else if (!strcmp(argv[i], "--library") && ((i + 1) < argc)) {
            library = argv[i + 1];
            i += 1;
        } else {
            if (file.empty())
                file = argv[i];
            else
                basePath = argv[i];
		}
	}

    if (file.empty())
    {
        std::cerr << "No file to decrypt" << std::endl;
        return 1;
    }

    // If output directory was not specified, use current directory
    if (basePath.empty())
    {
        basePath = ".";
        basePath += filesystem::path::preferred_separator;
    }

    if (!library.empty())
    {
        crypto.connectLibrary(library);
    }
    else if (libraryRequired)
    {
        cerr << "Cryptographic library is required" << endl;
        print_usage(cerr);
        return 1;
    }

	crypto.rcpts[label] = {
        RcptInfo::ANY,
		{},
        secret,
        slot, key_id, key_label
	};
	ToolConf conf;
    unique_ptr<libcdoc::CDocReader> rdr(libcdoc::CDocReader::createReader(file, &conf, &crypto, nullptr));
    std::cout << "Reader created" << std::endl;
    std::vector<const libcdoc::Lock> locks = rdr->getLocks();
    for (const libcdoc::Lock& lock : locks) {
        if (lock.label == label)
        {
			std::vector<uint8_t> fmk;
			rdr->getFMK(fmk, lock);
            libcdoc::FileListConsumer fileWriter(basePath);
            rdr->decrypt(fmk, &fileWriter);
            // rdr->beginDecryption(fmk);
            // std::string name;
            // int64_t size;
            // while (rdr->nextFile(name, size) == libcdoc::OK) {
   //              std::cout << name << ":" << size << std::endl;
            // }
            break;
		}
	}
	return 0;
}

//
// cdoc-tool locks FILE
//

int locks(int argc, char *argv[])
{
    if (argc < 1)
    {
        print_usage(cerr);
        return 1;
    }
    unique_ptr<libcdoc::CDocReader> rdr(libcdoc::CDocReader::createReader(argv[0], nullptr, nullptr, nullptr));
    const std::vector<const libcdoc::Lock> locks = rdr->getLocks();
    for (const libcdoc::Lock& lock : locks) {
        cout << lock.label << endl;
	}
	return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(cerr);
        return 1;
    }

    cout << "Command: " << argv[1] << endl;
	if (!strcmp(argv[1], "encrypt")) {
		return encrypt(argc - 2, argv + 2);
	} else if (!strcmp(argv[1], "decrypt")) {
		return decrypt(argc - 2, argv + 2);
	} else if (!strcmp(argv[1], "locks")) {
		return locks(argc - 2, argv + 2);
	} else if(argc >= 5 && strcmp(argv[1], "encrypt") == 0) {
#if 0
		CDOC1Writer w(toUTF8(argv[argc-1]));
		for(int i = 2; i < argc - 1; ++i)
		{
			if (strcmp(argv[i], "-r") == 0)
			{
				w.addRecipient(readFile(toUTF8(argv[i + 1])));
				++i;
			}
			else
			{
				std::string inFile = toUTF8(argv[i]);
				size_t pos = inFile.find_last_of("/\\");
				w.addFile(pos == std::string::npos ? inFile : inFile.substr(pos + 1), "application/octet-stream", inFile);
			}
		}
		if(w.encrypt())
			std::cout << "Success" << std::endl;
		else
			std::cout << w.lastError() << std::endl;
#endif
	} else if(argc == 7 && strcmp(argv[1], "decrypt") == 0) {
#if 0
		std::unique_ptr<Token> token;
		if (strcmp(argv[2], "pkcs11") == 0)
			token.reset(new PKCS11Token(toUTF8(argv[3]), argv[4]));
		else if (strcmp(argv[2], "pkcs12") == 0)
			token.reset(new PKCS12Token(toUTF8(argv[3]), argv[4]));
#ifdef _WIN32
		else if (strcmp(argv[2], "win") == 0)
			token.reset(new WinToken(strcmp(argv[3], "ui") == 0, argv[4]));
#endif
		CDoc1Reader r(toUTF8(argv[5]));
		if(r.mimeType() == "http://www.sk.ee/DigiDoc/v1.3.0/digidoc.xsd")
		{
			for(const DDOCReader::File &file: DDOCReader::files(r.decryptData(token.get())))
				writeFile(toUTF8(argv[6]) + "/" + file.name, file.data);
		}
		else
			writeFile(toUTF8(argv[6]) + "/" + r.fileName(), r.decryptData(token.get()));
#endif
	}
	else
	{
        print_usage(cout);
        return 0;
	}
	return 0;
}
