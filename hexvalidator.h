#ifndef HEXVALIDATOR_H
#define HEXVALIDATOR_H
/// @file hexvalidator.h
/// @brief Header file for the HexValidator class.
/// @defgroup hexvalidator_h GUI Hex Validator
/// @{
#include <QValidator>
#include <QRegExp>

class HexValidator : public QValidator {
	Q_OBJECT
public:
	explicit HexValidator(QObject *parent = nullptr) : QValidator(parent) {}

	/// @brief Validates if string is a valid hexadecimal number. It should only contain digits (0-9) and letters (A-F, a-f).
	State validate(QString &input, int &pos) const override {
		Q_UNUSED(pos);

		QRegExp hexRegExp("^[0-9A-Fa-f]*$");
		if (hexRegExp.exactMatch(input)) {
			return Acceptable;
		} else {
			return Invalid;
		}
	}
};

/// @}
#endif // HEXVALIDATOR_H
