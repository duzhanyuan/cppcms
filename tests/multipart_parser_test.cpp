///////////////////////////////////////////////////////////////////////////////
//                                                                             
//  Copyright (C) 2008-2012  Artyom Beilis (Tonkikh) <artyomtnk@yahoo.com>     
//                                                                             
//  See accompanying file COPYING.TXT file for licensing details.
//
///////////////////////////////////////////////////////////////////////////////
//#define DEBUG_MULTIPART_PARSER
#include <iostream>
#include "multipart_parser.h"
#include <iostream>
#include "test.h"
#include <string.h>
#include <stdlib.h>

char const test_1_content[] = "multipart/form-data; boundary=xxyy";
char const test_1_file[] = 
"--xxyy\r\n"
"Content-Disposition: form-data; name=\"test1\"; filename=\"foo.txt\"\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"hello\r\n"
"\r\n"
"--xxyy\r\n"
"Content-Disposition: form-data; name=\"test2\"\r\n"
"\r\n"
"שלום\r\n"
"--xxyy\r\n"
"Content-Disposition: form-data; name=\"test3\"\r\n"
"\r\n"
"x\r"
"\r\n--xxyy\r\n"
"Content-Disposition: form-data; name=\"test4\"\r\n"
"\r\n"
"x\r\n-"
"\r\n--xxyy\r\n"
"Content-Disposition: form-data; name=\"test5\"\r\n"
"\r\n"
"x\r\n--"
"\r\n--xxyy\r\n"
"Content-Disposition: form-data; name=\"test6\"\r\n"
"\r\n"
"x\r\n--x"
"\r\n--xxyy\r\n"
"Content-Disposition: form-data; name=\"test7\"\r\n"
"\r\n"
"x\r\n--x-"
"\r\n--xxyy--\r\n";

char const test_2_content[] = "multipart/form-data; boundary=-AbC";
char const test_2_file[] = 
"---AbC\r\n"
"Content-Disposition: form-data; name=\"test1\"  \r\n"
"\r\n"
"hello"
"\r\n"
"---AbC--\r\n";


std::string getcontent(std::istream &in)
{
	std::string res;
	while(!in.eof()) {
		char c;
		in.get(c);
		if(in.gcount() == 1)
			res+=c;
	}
	in.clear();
	in.seekg(0);
	return res;
}

std::string getcontent(cppcms::http::file &file)
{
	long long len = file.size();
	std::string res = getcontent(file.data());
	TEST(int(res.size()) == len);
	return res;
}
std::string getcontent(booster::shared_ptr<cppcms::http::file> const &p)
{
	return getcontent(*p);
}

struct random_consumer {
	random_consumer(int bs,cppcms::impl::multipart_parser &p,cppcms::impl::multipart_parser::files_type &f,std::vector<std::string> *cont = 0) :
		block_size(bs),
		parser(&p),
		files(&f),
		strings(cont)
	{
	}
	int block_size;
	cppcms::impl::multipart_parser *parser;
	cppcms::impl::multipart_parser::files_type *files;
	std::vector<std::string> *strings;
	cppcms::impl::multipart_parser::parsing_result_type operator()(char const *buffer,int size)
	{
		while(size > 0) {
			int block = 1;
			if(block_size > 1)
				block = (rand() % (block_size-1) + 1);
			else if(block_size < 0) 
				block=size;

			if(block > size)
				block=size;
				
			cppcms::impl::multipart_parser::parsing_result_type res = cppcms::impl::multipart_parser::continue_input;
		
			char const *start = buffer;
			char const *end = start + block;
			while(start != end) {
				res = parser->consume(start,end);
				TEST(start<=end);
				if(res == cppcms::impl::multipart_parser::meta_ready) {
					TEST(parser->has_file());
					TEST(!parser->get_file().name().empty());
					TEST(parser->get_file().size()==0);
					continue;
				}
				else if(res == cppcms::impl::multipart_parser::content_partial) {
					TEST(parser->has_file());
					TEST(!parser->get_file().name().empty());
					continue;
				}
				else if(res ==  cppcms::impl::multipart_parser::content_ready) {
					TEST(!parser->has_file());
					if(strings)
						strings->push_back(getcontent(parser->last_file()));
					continue;
				}
				else
					break;
			}
			
			buffer+=block;
			size-=block;
			if(res==cppcms::impl::multipart_parser::eof) {
				TEST(start == end);
				*files = parser->get_files();
				return res;
			}
			if(res==cppcms::impl::multipart_parser::parsing_error) {
				return res;
			}
			TEST(	res==cppcms::impl::multipart_parser::continue_input 
				|| res==cppcms::impl::multipart_parser::meta_ready
				|| res==cppcms::impl::multipart_parser::content_partial
				|| res==cppcms::impl::multipart_parser::content_ready
				);
		}
		TEST(!"Never get there");
		return cppcms::impl::multipart_parser::eof;
	}

};

int main(int argc,char **argv)
{
	if(argc!=2) {
		std::cerr << "Required path to tests folder" << std::endl;
		return 1;
	}
	try {
		int block_size[]={ 1, -1, 5, 3, 10, 1000 };
		int max_mem_size[] = { 0, 3, 10000 };
		for(unsigned i=0;i<sizeof(block_size)/sizeof(block_size[0]);i++) {
			for(unsigned j=0;j<sizeof(max_mem_size)/sizeof(max_mem_size[0]);j++) {
				std::cout << "Testing for max-block-size = " << block_size[i] << " mem file size = " << max_mem_size[j] << std::endl;
				{
					cppcms::impl::multipart_parser parser("",max_mem_size[j]);
					TEST(parser.set_content_type(std::string(test_1_content)));
					cppcms::impl::multipart_parser::files_type files;
					std::vector<std::string> cts;
					random_consumer c(block_size[i],parser,files,&cts);
					TEST(c(test_1_file,strlen(test_1_file))==cppcms::impl::multipart_parser::eof);
					TEST(files.size()==7);
					TEST(files[0]->name()=="test1");
					TEST(files[0]->filename()=="foo.txt");
					TEST(files[0]->mime()=="text/plain");
					std::string content = getcontent(files[0]);
					TEST(content=="hello\r\n");
					TEST(cts.at(0) == content);
					TEST(files[1]->name()=="test2");
					TEST(files[1]->filename()=="");
					TEST(files[1]->mime()=="");
					TEST(getcontent(files[1])=="שלום");
					TEST(cts.at(1)=="שלום");
					TEST(getcontent(files[2])=="x\r");
					TEST(getcontent(files[3])=="x\r\n-");
					TEST(getcontent(files[4])=="x\r\n--");
					TEST(getcontent(files[5])=="x\r\n--x");
					TEST(getcontent(files[6])=="x\r\n--x-");
					TEST(cts.at(6)=="x\r\n--x-");
				}
				{
					cppcms::impl::multipart_parser parser("",max_mem_size[j]);
					TEST(parser.set_content_type(std::string(test_2_content)));
					cppcms::impl::multipart_parser::files_type files;
					random_consumer c(block_size[i],parser,files);
					TEST(c(test_2_file,strlen(test_2_file))==cppcms::impl::multipart_parser::eof);
					TEST(files.size()==1);
					TEST(files[0]->name()=="test1");
					TEST(files[0]->filename()=="");
					TEST(files[0]->mime()=="");
					std::string content = getcontent(files[0]);
					TEST(content=="hello");
				}
				std::string boundaries[4]= { 
					"multipart/form-data; boundary=---------------------------11395741071221114234100471568",
					"multipart/form-data; boundary=----WebKitFormBoundaryuYwgZwieYHQ3+AR8",
					"multipart/form-data; boundary=----------jrFLC4hxayof1KyJEgmmCw",
					"multipart/form-data; boundary=---------------------------7da3a5810028"
				};
				std::string filenames[4] = {
					"firefox",
					"chrome",
					"opera",
					"ie" 
				};
				std::string location[2] = {
					".",
					""
				};
				for(int loc=0;loc<2;loc++) {
					std::cout << "- location [" << location[loc] <<']'<< std::endl;
					for(int i=0;i<4;i++) {
						std::cout << "-- Browser " << filenames[i] << std::endl;
						std::string file_name = std::string(argv[1])+"/multipart/" + filenames[i]+".txt";
						std::ifstream file(file_name.c_str(),std::ifstream::binary);
						if(!file) {
							std::cerr << "Failed to open file " << file_name << std::endl;
							return 1;
						}
						std::string content = getcontent(file);
						cppcms::impl::multipart_parser parser(location[loc],max_mem_size[j]);
						TEST(parser.set_content_type(boundaries[i]));
						cppcms::impl::multipart_parser::files_type files;
						random_consumer c(block_size[i],parser,files);
						TEST(c(content.c_str(),content.size())==cppcms::impl::multipart_parser::eof);
						TEST(files.size()==3);
						TEST(files[0]->name()=="submit-name");
						TEST(files[0]->mime()=="" && files[0]->filename().empty());
						TEST(getcontent(files[0])=="שלום");
						TEST(files[1]->name()=="file");
						TEST(files[1]->mime()=="text/plain");
						if(i==3) // IE
							TEST(files[1]->filename()=="z:\\tmp\\שלום.txt" || files[1]->filename()=="z:tmpשלום.txt");
						else
							TEST(files[1]->filename()=="שלום.txt");
						
						TEST(getcontent(files[1])=="שלום עולם!\n");
						files[1]->save_to("test.txt");
						{
							std::ifstream tmp("test.txt");
							TEST(tmp);
							TEST(getcontent(tmp)=="שלום עולם!\n");
							tmp.close();
							std::remove("test.txt");
						}

						TEST(files[2]->name()=="submit");
						TEST(files[2]->mime().empty());
						TEST(files[2]->filename().empty());
						TEST(getcontent(files[2])=="Send");
					}
				}
			}
		}
		
	}
	catch(std::exception const &e) {
		std::cerr << "Fail: " <<e.what() << std::endl;
		return 1;
	}
	std::cout << "Ok" << std::endl;
	return 0;
}
