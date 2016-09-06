#pragma once

/*
 * 证书提供者，主要用于生成所需要的根证书和对请求的网站每次签发证书
 * 
 * 注：部分的代码，直接来自于openssl库中的函数
 * 
 */
#include <windows.h>
#include <openssl/x509.h>
#include <openssl/pkcs12.h>
class CertificateProvider
{
public:
    CertificateProvider();
    ~CertificateProvider();
    enum{
        FORMAT_ASN1=1,
        FORMAT_PEM=2,
    };

    enum{
        DEF_DAYS=30,
    };

public:
    static X509*        csr2crt(X509_REQ *x509_req, EVP_PKEY *pKey);
    static X509*        generate_certificate(EVP_PKEY * pkey, char*, char*, char*, int days=DEF_DAYS);
    static EVP_PKEY *   generate_keypair(int numofbits);  //生成密钥对
    static int          exportPriKey(EVP_PKEY *pKey,unsigned char *buf, int len);
    static int          importPriKey(EVP_PKEY **ppKey, unsigned char *buf, int len);
    static int          savePriKeytofile(EVP_PKEY *pkey, char*path);
    static int          addCert2WindowsAuth(unsigned char *buf_x509_der, int len_x509_der, const char* pos);
    static int          addCert2WindowsAuth(X509* x509, const char *pos);  
    static int          addCert2WindowsAuth(PKCS12*pkcs12, const char *pos,wchar_t* password);
    static int          exportx509(X509* x509, unsigned char* buf, int len);
    static int          importx509(X509**pX509, unsigned char* buf, int len);
    static int          x509_certify(X509*x,X509*xca, EVP_PKEY*pkey_ca); /*CA签名*/
    static int          saveX509tofile(X509* x509, char *path);
    static int          is_certexist(char* pszIssuer, char* pszCertStore, char* pszUsername);
    static int          is_certexist(X509 *x509, char *pszCertStore, wchar_t *pszpwd);   //是否存在
    static PKCS12*      x509topkcs12(X509* x509,EVP_PKEY *pkey,char *password,char* aname,X509*CA);    //x509转为pkcs12
    static int          pkcs12_getx509(PKCS12* pkcs12,char* pass,int len,X509**cert,EVP_PKEY**pkey,X509**CA);
    static void         del_certs(char *pszIssuer, char *pszCertStore, char *pszUsername);
    static PKCS12*      get_pkcs12fromWindowsAuth(wchar_t*pszpwd,char *pszIssuer,char*pszCertStore,char*pszUserName);
private:
    static int          rand_serial(BIGNUM *b, ASN1_INTEGER *ai);
    static int          do_X509_sign(X509 *x, EVP_PKEY *pkey, const EVP_MD *md,STACK_OF(OPENSSL_STRING) *sigopts);
    static int          pkey_ctrl_string(EVP_PKEY_CTX *ctx, const char *value);
    static int          do_sign_init(EVP_MD_CTX *ctx, EVP_PKEY *pkey, const EVP_MD *md, STACK_OF(OPENSSL_STRING) *sigopts);
    static X509 *       load_cert(const char *file, int format);
};