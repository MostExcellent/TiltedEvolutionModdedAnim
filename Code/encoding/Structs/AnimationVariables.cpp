#include <Structs/AnimationVariables.h>
#include <TiltedCore/Serialization.hpp>
#include <iostream>

bool AnimationVariables::operator==(const AnimationVariables& acRhs) const noexcept
{
    return Booleans == acRhs.Booleans && Integers == acRhs.Integers && Floats == acRhs.Floats;
}

bool AnimationVariables::operator!=(const AnimationVariables& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void AnimationVariables::Load(std::istream& aInput)
{
    size_t booleansSize, integersSize, floatsSize;
    aInput.read(reinterpret_cast<char*>(&booleansSize), sizeof(size_t));
    Booleans.resize(booleansSize);

    for (size_t i = 0; i < booleansSize; ++i)
    {
        char value;
        aInput.read(&value, sizeof(char));
        Booleans[i] = (value != 0);
    }

    aInput.read(reinterpret_cast<char*>(&integersSize), sizeof(size_t));
    Integers.resize(integersSize);
    aInput.read(reinterpret_cast<char*>(Integers.data()), Integers.size() * sizeof(uint32_t));

    aInput.read(reinterpret_cast<char*>(&floatsSize), sizeof(size_t));
    Floats.resize(floatsSize);
    aInput.read(reinterpret_cast<char*>(Floats.data()), Floats.size() * sizeof(float));
}



void AnimationVariables::Save(std::ostream& aOutput) const
{
    // First, write the size of each vector to know how many elements to read on load
    size_t booleansSize = Booleans.size();
    size_t integersSize = Integers.size();
    size_t floatsSize = Floats.size();

    aOutput.write(reinterpret_cast<const char*>(&booleansSize), sizeof(size_t));
    // Since std::vector<bool> is a special case, serialize its content as individual bytes
    for (bool boolean : Booleans)
    {
        char value = boolean ? 1 : 0; // Convert boolean to a byte
        aOutput.write(&value, sizeof(char));
    }

    aOutput.write(reinterpret_cast<const char*>(&integersSize), sizeof(size_t));
    // Direct bulk write for integers and floats as before
    if (!Integers.empty())
    {
        aOutput.write(reinterpret_cast<const char*>(Integers.data()), integersSize * sizeof(uint32_t));
    }

    aOutput.write(reinterpret_cast<const char*>(&floatsSize), sizeof(size_t));
    if (!Floats.empty())
    {
        aOutput.write(reinterpret_cast<const char*>(Floats.data()), floatsSize * sizeof(float));
    }
}

void AnimationVariables::GenerateDiff(const AnimationVariables& aPrevious, TiltedPhoques::Buffer::Writer& aWriter) const
{
    // Serialize sizes of vectors to know how many elements to expect during deserialization
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Booleans.size());
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Integers.size());
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Floats.size());

    // Booleans
    for (size_t i = 0; i < Booleans.size(); ++i)
    {
        bool previousState =
            i < aPrevious.Booleans.size() ? aPrevious.Booleans[i] : false; // Default to false if out of range
        aWriter.WriteBits(Booleans[i] != previousState, 1);                // 1 bit for change flag
        if (Booleans[i] != previousState)
        {
            aWriter.WriteBits(Booleans[i], 1); // Actual value if changed
        }
    }

    // Integers
    for (size_t i = 0; i < Integers.size(); ++i)
    {
        uint32_t previousValue =
            i < aPrevious.Integers.size() ? aPrevious.Integers[i] : 0; // Default to 0 if out of range
        aWriter.WriteBits(Integers[i] != previousValue, 1);            // 1 bit for change flag
        if (Integers[i] != previousValue)
        {
            TiltedPhoques::Serialization::WriteVarInt(aWriter, Integers[i]); // Serialize the integer if changed
        }
    }

    // Floats
    for (size_t i = 0; i < Floats.size(); ++i)
    {
        float previousValue =
            i < aPrevious.Floats.size() ? aPrevious.Floats[i] : 0.0f; // Default to 0.0 if out of range
        bool hasChanged = Floats[i] != previousValue;
        aWriter.WriteBits(hasChanged, 1); // 1 bit for change flag
        if (hasChanged)
        {
            // Serialize the floating-point number if changed
            uint32_t floatBits = *reinterpret_cast<const uint32_t*>(&Floats[i]);
            aWriter.WriteBits(floatBits, 32);
        }
    }
}

void AnimationVariables::ApplyDiff(TiltedPhoques::Buffer::Reader& aReader)
{
    const auto cBooleansSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    Booleans.resize(cBooleansSize);

    const auto cIntegersSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    Integers.resize(cIntegersSize);

    const auto cFloatsSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    Floats.resize(cFloatsSize);

    // Booleans
    for (size_t i = 0; i < cBooleansSize; ++i)
    {
        uint64_t changeFlag = 0;
        aReader.ReadBits(changeFlag, 1);
        if (changeFlag)
        {
            uint64_t bitValue = 0;
            aReader.ReadBits(bitValue, 1);
            Booleans[i] = (bitValue != 0);
        }
    }

    // Integers
    for (auto& value : Integers)
    {
        uint64_t changeFlag = 0;
        aReader.ReadBits(changeFlag, 1);
        if (changeFlag)
        {
            value = TiltedPhoques::Serialization::ReadVarInt(aReader);
        }
    }

    // Floats
    for (auto& value : Floats)
    {
        uint64_t changeFlag = 0;
        aReader.ReadBits(changeFlag, 1);
        if (changeFlag)
        {
            uint64_t tmp = 0;
            aReader.ReadBits(tmp, 32);
            uint32_t data = tmp & 0xFFFFFFFF;
            value = *reinterpret_cast<float*>(&data);
        }
    }
}
