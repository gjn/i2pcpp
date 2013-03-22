#include <iostream>

#include <botan/botan.h>
#include <botan/dsa.h>
#include <botan/elgamal.h>

#include "Log.h"
#include "transport/UDPTransport.h"
#include "util/Base64.h"

int main()
{
	using namespace i2pcpp;

	Log::initialize();

	Botan::LibraryInitializer init;
	Botan::AutoSeeded_RNG rng;

	Botan::DL_Group elg_group("modp/ietf/2048");
	Botan::ElGamal_PrivateKey elg_key(rng, elg_group);

	Botan::BigInt p("0x9C05B2AA960D9B97B8931963C9CC9E8C3026E9B8ED92FAD0A69CC886D5BF8015FCADAE31A0AD18FAB3F01B00A358DE237655C4964AFAA2B337E96AD316B9FB1CC564B5AEC5B69A9FF6C3E4548707FEF8503D91DD8602E867E6D35D2235C1869CE2479C3B9D5401DE04E0727FB33D6511285D4CF29538D9E3B6051F5B22CC1C93");
	Botan::BigInt q("0xA5DFC28FEF4CA1E286744CD8EED9D29D684046B7");
	Botan::BigInt g("0xC1F4D27D40093B429E962D7223824E0BBC47E7C832A39236FC683AF84889581075FF9082ED32353D4374D7301CDA1D23C431F4698599DDA02451824FF369752593647CC3DDC197DE985E43D136CDCFC6BD5409CD2F450821142A5E6F8EB1C3AB5D0484B8129FCF17BCE4F7F33321C3CB3DBB14A905E7B2B3E93BE4708CBCC82");
	Botan::DL_Group dsa_group(p, q, g);
	Botan::DSA_PrivateKey dsa_key(rng, dsa_group);

	Botan::BigInt encryptionKeyPublic, signingKeyPublic;
	encryptionKeyPublic = elg_key.get_y();
	signingKeyPublic = dsa_key.get_y();
	ByteArray encryptionKeyBytes = Botan::BigInt::encode(encryptionKeyPublic), signingKeyBytes = Botan::BigInt::encode(signingKeyPublic);

	UDPTransport u(dsa_key, RouterIdentity(encryptionKeyBytes, signingKeyBytes, Certificate()));
	u.start(Endpoint("127.0.0.1", 12345));

	int x;
	std::cin >> x;

	return 0;
}
