#!/usr/bin/ruby

sys = ''
rev = 0

if RUBY_PLATFORM =~ /win32/
	sys = `ver`.strip
else
	sys = `uname -a`.strip
end

# str = `svn info`
str = `LANG=C; svn info`

str.each_line {|line|
	if line =~ /^Revision\: ([0-9]+)$/
		rev = $1.to_i
	end
}

puts "char const *build_timestamp = \"#{Time.new.to_s}\";"
puts "char const *build_system = \"#{sys}\";"
puts "int build_revision = #{rev};"

