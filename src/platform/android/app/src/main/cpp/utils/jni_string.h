#include <jni.h>

class JniString {
public:
	JniString(JNIEnv* env, jstring jstr)
	    : m_env(env), m_jstr(jstr), m_str(jstr ? env->GetStringUTFChars(jstr, nullptr) : nullptr) {}

	~JniString() {
		if (m_str && m_jstr) {
			m_env->ReleaseStringUTFChars(m_jstr, m_str);
		}
	}

	operator const char*() const { return m_str; }
	[[nodiscard]] const char* c_str() const { return m_str; }

	JniString(const JniString&) = delete;
	JniString& operator=(const JniString&) = delete;

private:
	JNIEnv* m_env;
	jstring m_jstr;
	const char* m_str;
};