#pragma once

#include <iostream>
#include <map>

#include "platform.hpp"
#include "string_id.hpp"
#include "printer.hpp"


namespace vm
{
    struct FLocalDefinition;
    struct ModuleRef;
    struct BinFileHeader;
    struct FNativeFunctionEntry;

    /**
	 * @brief The single record in the global definition table
	 */
    struct FLocalDefinition
    {
        FLocalDefinition() = default;
        FLocalDefinition(const FLocalDefinition& other) = default;
        FLocalDefinition(StringId type, PTRINT ptr) : ptr(ptr), type(type) {}

        virtual bool is_global() { return false; }
        /**
	     * @brief Check if the definition of given type
	     * @param type - the type's string id
	     * @return True if type is matched
	     */
        bool is_type(StringId type) const { return type == type; }

        bool is_integer() const { return type == SID("int32"); }
        bool is_float() const { return type == SID("float"); }
        bool is_string_id() const { return type == SID("StringId"); }
        bool is_lambda() const { return type == SID("lambda"); }

        /** Assign another FLocalDefinition */
        FLocalDefinition& operator=(const FLocalDefinition& other);

        std::string to_string() const;

        /** The pointer to definition */
        PTRINT ptr;
        /** The type of definition */
        StringId type;
    };
    //--------------------------------------------------------------------------
	// The global environment 
	//--------------------------------------------------------------------------

    /**
     * @brief The single record in the global definition table
     */
    struct FGlobalDefinition :  FLocalDefinition
    {
        FGlobalDefinition() = default;
        FGlobalDefinition(const FGlobalDefinition& other) = default;
        FGlobalDefinition(const StringId type, const FNativeFunctionEntry& ptr);
        FGlobalDefinition(const StringId type, const PTRINT ptr, const ModuleRef* moduleRef);

        bool is_global() override { return true; }

        /**
    	 * @brief Verify if the definition is still available
    	 * @return True for valid definition
    	 */
    	bool is_valid() const;

        /**
         * @brief Assign another FLocalDefinition 
         * @param other - right side variable 
         * @return the result 
         */
        FGlobalDefinition& operator=(const FGlobalDefinition& other);

        /**
         * @brief Convert to the string
         * @return the string value
         */
        std::string to_string() const;

        /** Reference to the file definition */
        const ModuleRef* module_ref{};

        /** generation number */
        u32 generation{};
    };


    /**
     * @brief The pointer in the local environment
     */
    struct FGlobalEnvironment 
    {
        /**
         * @brief Find the element in the environment
         * @param name - the element name
         * @return The pointer or null
         */
        FGlobalDefinition* lookup(const StringId name);
        /**
		 * @brief Find the element in the environment
		 * @param name - the element name
		 * @param type - the type of expected object
		 * @return The pointer or null
		 */
        FGlobalDefinition* lookup(const StringId name, const StringId type);

        /**
         * @brief Define the element in the environment
         * @param name - The name of element
         * @param data - The data for element
         */
        void define(const StringId name, const FGlobalDefinition& data)
        {
            table[name] = data;
        }

        void dump() const
        {
            std::cout << to_string() << std::endl;
            for (auto& t : table)
                std::cout << t.second.to_string() << std::endl; 
        }


        std::string to_string() const
        {
            return std::format("#TEnvironment <size: {0}>",
                table.size());
        }

    private:

        /**
         * @brief The table of elements in the environment
         */
        std::map<StringId, FGlobalDefinition> table;
    };

    //--------------------------------------------------------------------------
	// The local environment 
	//--------------------------------------------------------------------------

    /**
	 * @brief The pointer in the local environment
	 */
    struct FLocalEnvironment  {
        /**
         * @brief Find the element in the environment
         * @param name - the element name
         * @param recursive - search in the global environment
         * @return The pointer or null
         */
        FLocalDefinition* lookup(const StringId name, bool recursive = false);
        /**
         * @brief Find the element in the environment
         * @param name - the element name
         * @param type - the type of definition
         * @param recursive - look at global environment too
         * @return The pointer or null
         */
        FLocalDefinition* lookup(const StringId name, const StringId type, bool recursive = false);
        /**
         * @brief Define the element in the environment
         * @param name - The name of element
         * @param data - The data for element
         */
        void define(const StringId name, const FLocalDefinition& data)
        {
            table[name] = data;
        }

        void dump() const
        {
            std::cout << "Dump environment..." << std::endl;
            std::cout << to_string() << std::endl;
            for (auto& t : table)
                std::cout << t.second.to_string() << std::endl;
        }

        std::string to_string() const
        {
            return std::format("#TEnvironment <size: {0}>",
                table.size());
        }

    private:

        /**
         * @brief The table of elements in the environment
         */
        std::map<StringId, FLocalDefinition> table;
    };

    /**
     * @brief There is the global environment 
     */
	extern 	FGlobalEnvironment g_environment;

}
