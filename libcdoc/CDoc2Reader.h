#ifndef __CDOC2_READER_H__
#define __CDOC2_READER_H__

#include "CDocReader.h"

class CDoc2Reader final: public libcdoc::CDocReader {
public:
	~CDoc2Reader() final;

	const std::vector<libcdoc::Lock *>& getLocks() override final;
	const libcdoc::Lock *getDecryptionLock(const std::vector<uint8_t>& cert) final;
	int getFMK(std::vector<uint8_t>& fmk, const libcdoc::Lock *lock) override final;
	int decrypt(const std::vector<uint8_t>& fmk, libcdoc::MultiDataConsumer *consumer) override final;

	// Pull interface
	int beginDecryption(const std::vector<uint8_t>& fmk) override final;
	int nextFile(std::string& name, int64_t& size) override final;
	int64_t read(uint8_t *dst, size_t size) override final;
	int finishDecryption() override final;

	CDoc2Reader(libcdoc::DataSource *src, bool take_ownership = false);
	CDoc2Reader(const std::string &path);

	static bool isCDoc2File(const std::string& path);
private:
	struct Private;

	std::unique_ptr<Private> priv;
};

#endif