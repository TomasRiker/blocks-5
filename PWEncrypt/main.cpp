#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#endif

void generatePrimes(unsigned int* p_out,
					unsigned int maxNum)
{
	if(!maxNum) return;

	p_out[0] = 2;

	unsigned int n = 3;
	unsigned int numPrimes = 1;
	while(numPrimes < maxNum)
	{
		// Is n a prime number?
		bool isPrime = true;
		for(unsigned int i = 0; i < numPrimes; i++)
		{
			if(!(n % p_out[i]))
			{
				isPrime = false;
				break;
			}
		}

		if(isPrime)
		{
			p_out[numPrimes] = n;
			numPrimes++;
		}

		n++;
	}
}

void factorize(unsigned int n,
			   unsigned int* p_outPrimes,
			   unsigned int* p_outPowers,
			   unsigned int* p_outNumTerms,
			   const unsigned int* p_primes)
{
	unsigned int i = 0;
	unsigned int index = 0;
	while(n != 1)
	{
		// Is the i-th prime a factor?
		if(!(n % p_primes[i]))
		{
			p_outPrimes[index] = i;
			p_outPowers[index] = 1;
			n /= p_primes[i];

			// Is it a factor more than once?
			while(!(n % p_primes[i]))
			{
				p_outPowers[index]++;
				n /= p_primes[i];
			}

			index++;
		}

		i++;
	}

	*p_outNumTerms = index;
}

void toBase62(unsigned int n,
			  char* p_out)
{
	const unsigned int base[7] = {916132832, 14776336, 238328, 63844, 3844, 62, 1};
	for(unsigned int i = 0; i < 7; i++)
	{
		unsigned int c = n / base[i];
		n -= c * base[i];

		// output encoding
		if(c < 10) p_out[i] = '0' + c;
		else if(c < 36) p_out[i] = 'A' + (c - 10);
		else p_out[i] = 'a' + (c - 36);
	}

	p_out[7] = 0;
}

unsigned int fromBase62(const char* p_in)
{
	const unsigned int base[7] = {916132832, 14776336, 238328, 63844, 3844, 62, 1};
	unsigned int n = 0;
	for(unsigned int i = 0; i < 7; i++)
	{
		// decoding
		if(p_in[i] >= 'a') n += (36 + p_in[i] - 'a') * base[i];
		else if(p_in[i] >= 'A') n += (10 + p_in[i] - 'A') * base[i];
		else n += (p_in[i] - '0') * base[i];
	}

	return n;
}

void encryptPassword(const char* p_in,
					 char* p_out,
					 const unsigned int* p_primes)
{
	char out[256] = "";
	unsigned int index = 0;
	unsigned int sum = 0;

	for(unsigned int i = 0; i < strlen(p_in); i++)
	{
		unsigned int c = static_cast<unsigned int>(p_in[i]);
		c += i * 7;
		sum += c;

		// factorize the letter
		unsigned int primes[256], powers[256], numTerms;
		factorize(c, primes, powers, &numTerms, p_primes);

		// write the number of terms
		out[index++] = static_cast<char>(numTerms) ^ 0xB6;

		// write the primes and their powers
		for(unsigned int j = 0; j < numTerms; j++)
		{
			out[index++] = static_cast<char>(primes[j]) ^ 0x4D;
			out[index++] = static_cast<char>(powers[j]) ^ 0xE9;
		}
	}

	// append the padding bytes
	out[index] = 0;
	out[index++] ^= 0xB6;
	srand(sum);
	while(index % 4) out[index++] = rand() % 256;
	out[index] = 0;

	char readable[1024] = "";

	for(unsigned int i = 0, shift = 0; i < index; i += 4, shift++)
	{
		// always process 4 bytes together
		unsigned int c = *(reinterpret_cast<unsigned int*>(&out[i]));

		// the xor pattern varies
		unsigned int pattern = (0x958B47A6 << (shift % 31)) ^ (0x8D4BA2D4 >> (shift % 17));
		c ^= pattern;

		// write it out in base 62
		char b62[8] = "";
		toBase62(c, b62);
		strcat(readable, b62);
	}

	strcpy(p_out, readable);
}

void decryptPassword(const char* p_in,
					 char* p_out,
					 const unsigned int* p_primes)
{
	char step1[1024] = "";
	unsigned int length1 = static_cast<unsigned int>(strlen(p_in) / 7) * 4;
	for(unsigned int i = 0, shift = 0; i < strlen(p_in); i += 7, shift++)
	{
		// always turn 7 base-62 characters into one 32-bit integer
		unsigned int n = fromBase62(&p_in[i]);

		// decrypt
		unsigned int pattern = (0x958B47A6 << (shift % 31)) ^ (0x8D4BA2D4 >> (shift % 17));
		n ^= pattern;

		// write it
		*(reinterpret_cast<unsigned int*>(&step1[shift * 4])) = n;
	}

	char step2[256] = "";

	unsigned int indexIn = 0, indexOut = 0;
	while(true)
	{
		// read the number of terms and decrypt it
		unsigned char numTerms = step1[indexIn++] ^ 0xB6;
		if(!numTerms) break;

		// read the primes and their powers and decrypt them
		unsigned int c = 1;
		for(unsigned int i = 0; i < numTerms; i++)
		{
			unsigned char prime = step1[indexIn++] ^ 0x4D;
			unsigned char power = step1[indexIn++] ^ 0xE9;

			// multiply the power in
			for(unsigned int j = 0; j < power; j++) c *= p_primes[prime];
		}

		// decrypt the letter and write it
		c -= indexOut * 7;
		step2[indexOut++] = static_cast<char>(c);
	}

	step2[indexOut] = 0;
	strcpy(p_out, step2);
}

int main()
{
	unsigned int primes[256] = {0};
	generatePrimes(primes, 256);

	char in[256] = "";
	printf("Password: ");

	// gets() never had a length check and has been removed from the Universal
	// CRT. fgets reads at most sizeof(in)-1 characters, but leaves the
	// newline standing.
	if(!fgets(in, sizeof(in), stdin)) in[0] = 0;
	size_t inLength = strlen(in);
	while(inLength && (in[inLength - 1] == '\n' || in[inLength - 1] == '\r')) in[--inLength] = 0;

	char out[1024] = "";
	encryptPassword(in, out, primes);

#ifdef _WIN32
	char tempPath[256] = "";
	GetTempPathA(sizeof(tempPath), tempPath);
	char tempFilename[256] = "";
	sprintf(tempFilename, "%spwencrypt.txt", tempPath);
	FILE* p_file = fopen(tempFilename, "at");
	fprintf(p_file, "Password:  %s\nEncrypted: %s\n\n", in, out);
	fclose(p_file);
	char cmd[256] = "";
	sprintf(cmd, "NOTEPAD.EXE \"%s\"", tempFilename);
	system(cmd);
#else
	printf("Password:  %s\nEncrypted: %s\n", in, out);
#endif

	return 0;
}